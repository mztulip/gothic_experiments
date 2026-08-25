#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <zenkit/World.hh>
#include <zenkit/Stream.hh>
#include <zenkit/Mesh.hh>
#include <zenkit/vobs/Light.hh>
#include <zenkit/vobs/VirtualObject.hh>

#include <cstdio>

#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <random>

#include "info_render.hpp"
#include "camera.hpp"
#include "light_correction.hpp"
#include "shader_utils.hpp"

// ---------------------------------------------------------------------
// Wczytywanie prawdziwych swiatel z pliku .ZEN (ZenKit) - te same wartosci
// Range/kolor/preset co w grze, 
// ---------------------------------------------------------------------
struct LoadedLight {
  glm::vec3   pos;
  float       range;
  glm::vec3   color;
  std::string preset;
  };

static inline glm::vec3 zenPosToGL(float x, float y, float z)
{
  // ZenGin (Gothic) uzywa ukladu lewoskretnego, OpenGL prawoskretnego.
  // Negujemy DOKLADNIE JEDNA os, aby zamienic chiralnosc - tutaj X.
  // Jesli po tescie okaze sie, ze to zla os, zmien negacje na Y lub Z
  // (ale zawsze tylko jedna naraz - negacja dwoch osi to obrot, nie odbicie).
  return glm::vec3(-x, y, z);
}

static void walkVobsForLights(const std::shared_ptr<zenkit::VirtualObject>& vob,
                               std::vector<LoadedLight>& out, int& skippedStatic)
{
  if(vob->type==zenkit::VirtualObjectType::zCVobLight)
  {
    const auto& l = static_cast<const zenkit::VLight&>(*vob);
    if(l.is_static)
    {
      ++skippedStatic;
    }
    else
    {
      LoadedLight ll;
      ll.pos     = zenPosToGL(l.position.x, l.position.y, l.position.z);
      ll.range   = l.range;
      ll.color   = {l.color.r/255.f, l.color.g/255.f, l.color.b/255.f};
      ll.preset  = l.preset;
      out.push_back(ll);
    }
  }
  for(auto& c : vob->children)
    walkVobsForLights(c, out, skippedStatic);
}

static std::vector<LoadedLight> loadLightsFromZen(const std::string& path) {
  std::vector<LoadedLight> out;
  int skippedStatic = 0;
  try {
    auto reader = zenkit::Read::from(path);
    zenkit::World world;
    world.load(reader.get());
    for(auto& vob : world.world_vobs)
      walkVobsForLights(vob, out, skippedStatic);
    }
  catch(const std::exception& e) {
    fprintf(stderr, "Nie udalo sie wczytac %s: %s\n", path.c_str(), e.what());
    return {};
    }
  printf("Wczytano %zu dynamicznych swiatel z %s (pominieto %d statycznych - sa baked w vertex colors)\n",
         out.size(), path.c_str(), skippedStatic);
  for(auto& l : out)
    printf("  preset=%-16s range=%6.1f pos=(%.1f, %.1f, %.1f)\n",
           l.preset.empty() ? "(brak nazwy)" : l.preset.c_str(),
           l.range, l.pos.x, l.pos.y, l.pos.z);
  return out;
  }

// ---------------------------------------------------------------------
// Presety swiatla - zmierzone/przyjete wartosci z HELMS.ZEN (patrz rozmowa)
// ---------------------------------------------------------------------
struct Preset {
  const char* name;
  float       range;
  glm::vec3   color;      // baza koloru (0..1)
  };

static const Preset PRESETS[] = {
  {"FIRESMALL",  200.f, {1.00f, 0.29f, 0.00f}}, // 255,73,0
  {"FIRE",       300.f, {1.00f, 0.00f, 0.00f}}, // klatka 255,0,0 (najbardziej "problematyczna")
  {"JUSTWHITE",  700.f,  {1.00f, 1.00f, 0.68f}}, // 255,255,173
  {"WHITEBLEND",2000.f,  {1.00f, 1.00f, 0.58f}}, // 255,255,148
  {"AURA_650",   650.f,  {0.00f, 0.00f, 0.55f}}, // 0,0,139
  {"AURA_3000", 3000.f, {0.32f, 0.66f, 0.84f}}, // 81,168,214
  };


static int g_presetIdx = 1; // start na FIRE - to ten najbardziej sporny

// ---------------------------------------------------------------------
// Shadery - GLSL wklejony jako string, logika fragmentow 1:1 z light.frag
// ---------------------------------------------------------------------
static const char* VERT_SRC = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform bool  uIsFog;
uniform float uPointSizeBase;

out vec3 vWorldPos;
out vec3 vNormal;

void main() {
  vec4 world = uModel * vec4(aPos, 1.0);
  vWorldPos = world.xyz;
  vNormal   = mat3(uModel) * aNormal;
  vec4 viewPos = uView * world;
  gl_Position = uProj * viewPos;

  if(uIsFog) {
    float dist = length(viewPos.xyz);
    gl_PointSize = clamp(uPointSizeBase * (300.0/max(dist,1.0)), 2.0, 48.0);
  //gl_PointSize = 3.0; 

    // Kamera patrzy w kierunku -Z (konwencja OpenGL/view space).
    // Punkty zbyt blisko lub za kamera (viewPos.z blisko/powyzej 0) daja
    // zdegenerowane wspolrzedne po projekcji/dzieleniu przez w - GL_POINTS
    // nie sa clipowane na near-plane tak jak trojkaty, wiec taki punkt
    // potrafi wyrenderowac sie jako ogromny sprite na cala scene.
    // Wypychamy go recznie poza clip-space, zeby GPU go odrzucil.
    if(viewPos.z > -10.0) {
      gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
      }
    }
  }
)GLSL";

static const char* FRAG_SRC = R"GLSL(
    #version 330 core
    in vec3 vWorldPos;
    in vec3 vNormal;
    out vec4 FragColor;

    uniform vec3  uAlbedo; //Określa kolor tła
    uniform vec3  uLightPos;
    uniform vec3  uLightColor;
    uniform float uRange;
    uniform float uLightIntensity;
    uniform int   uFormulaMode;   // 0 = linia (replika oryginalu), 1 = obecna (Karis+Range^2)
    uniform int   uTonemap;       // 0 = brak, 1 = ACES (per-kanal, Narkowicz fit)
    uniform bool  uIsMarker;      // true dla kostki-znacznika swiatla - rysuj bez oswietlenia
    uniform int   uAmbientOnly;   // 1 = tylko ambient (pierwszy przebieg), 0 = tylko wklad tego swiatla (przebiegi addytywne)
    uniform bool  uIsFog;
    uniform float uFogDensity;

    //Academy Color Encoding System — system zarządzania kolorem opracowany przez Academy of Motion Picture Arts and Sciences
    //ta funkcja bierze kolor i ściska go do zakresu 0-1
    vec3 ACESFilm(vec3 x)
    {
        // Narkowicz 2015, popularna aproksymacja ACES stosowana per-kanal -
        // dokladnie ten sam charakter co domyslny tonemapping.glsl w OpenGothic.
        float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
        return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
    }

    vec3 acesTonemapInv(vec3 x)
    {
      float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
      return (-0.59 * x + 0.03 - sqrt(-1.0127 * x*x + 1.3702 * x + 0.0009)) / (2.0 * (2.43*x - 2.51));
    }
        

    vec3 srgbDecode(vec3 c) { return pow(c, vec3(2.2)); }

    vec3 textureAlbedo(vec3 rgb)
    {
      vec3 linear = srgbDecode(rgb);
      return acesTonemapInv(linear*0.78+0.001) * 5.0;
    }

    void main()
    {
        //Jeśli akurat rysujemy markery świateł
        //To ma on kolor źródła światła
        if(uIsMarker)
        {
            FragColor = vec4(uLightColor, 1.0);
            return;
        }

        float fogAlpha = 1.0;
        if(uIsFog)
        {
            //gl_PointCoord istnieje tylko dla GL_POINT jest to vec2 o zakresie od 0 do góry do dołu. Bo punkt może mieć wymiary w pixelach pewne i to się odnosi do tego punktu
            vec2 c = gl_PointCoord*2.0 - 1.0; //Przeskalowanie aby zakres był -1 do 1
            float r2 = dot(c,c); //odległość od środka punktu
            if(r2 > 1.0) discard; //odrzucamy wszystko powyżej 1 więc mamy koło
            fogAlpha = 1.0 - r2; //jest to współczynnik do przyciemniania im bardziej od środka. Wiec kropka mgły jest najjaśniejsza w srodku i mniej na zewnątrz.
            
            // --- DEBUG: wymuszony jaskrawy kolor, ignorujemy density/atten/tonemap ---
            //FragColor = vec4(1.0, 0.0, 1.0, 1.0); // magenta
            //return;
        }

        vec3 hdrColor;

        if(uAmbientOnly==1) //rysujemy mesh świata
        {
            // pierwszy przebieg (bez blendingu) - tylko ambient, ustawia tez depth buffer
          //  hdrColor = uAlbedo * 0.03;
            //hdrColor = uAlbedo * 0.50;
              float skyLight = max(0.0, normalize(vNormal).y) * 0.15; // gorne powierzchnie troche jasniejsze
              hdrColor = uAlbedo * (0.08 + skyLight);
        } 
        else //rysujemy źródła światła i ich markery i całą reszte
        {
            // kolejne przebiegi (addytywny blending GL_ONE,GL_ONE) - wklad TYLKO tego
            // jednego swiatla, bez ambientu (juz doliczony raz w pierwszym przebiegu)
            vec3  normal = normalize(vNormal);
            vec3  ldir   = uLightPos - vWorldPos;
            float dist   = length(ldir);
            //To uwzglednia kąt padania swiatla na powierzchnie
            float lambert = uIsFog ? 1.0 : max(0.0, dot(normalize(ldir), normal));

            float atten;
            if(uFormulaMode==0) //to tryb wyswietlania jak w oryginalnym silniku Gothic z D3D7, przełączane klawiszem M
            {
                // "linia": replika zmierzonego oryginalu D3D7/8 - 1/(Att1*d), Att1=0.009,
                // z twardym cap blisko zrodla (0.02) i twardym odcieciem na Range.
                const float ATT1 = 0.009;
                if(dist>uRange)
                {
                    atten = 0.0;
                }
                else
                {
                    atten = 1.0/max(ATT1*dist, 0.02);
                }
            }
            else
            {
              float distanceSquare = dot(ldir, ldir);
              float factor = distanceSquare / (uRange * uRange);

              if(factor > 1.0)
                  atten = 0.0;
              else
              {
                float smoothFactor = max(1.0 - factor * factor, 0.0);
                atten = (1.0 / max(factor, 0.005)) * (smoothFactor * smoothFactor);  // <- bez "lambert /"
                atten *= uLightIntensity;
              }
            }

            vec3 linear = textureAlbedo(uAlbedo);
            hdrColor = linear * uLightColor * lambert * atten * 0.25;

            //Jeśli rysujemy kropki mgły to trzeba uwzględnić fogALpha 
            if(uIsFog) hdrColor *= uFogDensity * fogAlpha;

        }

        vec3 outColor;
        if(uTonemap==1) //Przełączanie klawiszem T
            outColor = ACESFilm(hdrColor);
        else
            outColor = clamp(hdrColor, 0.0, 1.0); // zwykly LDR clamp - jak D3D7/8

        FragColor = vec4(outColor, 1.0);
    }
)GLSL";



// ---------------------------------------------------------------------
// Pomoce do budowy geometrii (podloga, kostka)
// ---------------------------------------------------------------------
struct Vertex { glm::vec3 pos; glm::vec3 normal; };

// Wczytuje statyczna siatke poziomu (BEZ tekstur/materialow - tylko geometria
// + normalne) z pliku .ZEN, do wyswietlania jednym, stalym uAlbedo (tak samo
// jak nasza plaska podloga). polygons.vertex_indices jest juz otrojkatowane
// przez ZenKit - trzy kolejne wartosci = jeden trojkat. Normalne bierzemy
// wprost z danych (VertexFeature::normal), nie liczymy ich sami.
static std::vector<Vertex> loadWorldMeshFromZen(const std::string& path)
{
  std::vector<Vertex> out;
  try
  {
    auto reader = zenkit::Read::from(path);
    zenkit::World world;
    world.load(reader.get());

    const zenkit::Mesh& mesh = world.world_mesh;
    const auto& vidx = mesh.polygons.vertex_indices;
    const auto& fidx = mesh.polygons.feature_indices;

    if(vidx.size()!=fidx.size() || vidx.size()%3!=0)
    {
      fprintf(stderr, "Nieoczekiwany uklad danych siatki (vidx=%zu fidx=%zu) - pomijam siatke.\n",
              vidx.size(), fidx.size());
      return {};
    }

    out.reserve(vidx.size());

    auto pushVertex = [&](size_t idx)
    {
      const auto& p = mesh.vertices[vidx[idx]];
      const auto& n = mesh.features[fidx[idx]].normal;
      glm::vec3 pos = zenPosToGL(p.x, p.y, p.z);
      glm::vec3 nrm = zenPosToGL(n.x, n.y, n.z);   // ta sama negacja dla normalnych - to poprawne dla odbicia diagonalnego
      out.push_back({pos, nrm});
    };

    for(size_t i = 0; i + 2 < vidx.size(); i += 3)
    {
      // Zamiana kolejnosci 2. i 3. wierzcholka kompensuje odwrocenie
      // "skretnosci" trojkata spowodowane negacja jednej osi.
      pushVertex(i+0);
      pushVertex(i+2);
      pushVertex(i+1);
    }
  }
  catch(const std::exception& e)
  {
    fprintf(stderr, "Nie udalo sie wczytac siatki z %s: %s\n", path.c_str(), e.what());
    return {};
  }

  printf("Wczytano siatke swiata: %zu trojkatow\n", out.size()/3);
  return out;
}

static void walkVobsForStartPoint(const std::shared_ptr<zenkit::VirtualObject>& vob,
                                   bool& found, glm::vec3& outPos)
{
  if(found) return;
  if(vob->type==zenkit::VirtualObjectType::zCVobStartpoint)
  {
    outPos = zenPosToGL(vob->position.x, vob->position.y, vob->position.z);
    found = true;
    return;
  }
  for(auto& c : vob->children)
  {
    walkVobsForStartPoint(c, found, outPos);
    if(found) return;
  }
}

// Szuka pierwszego zCVobStartpoint w pliku .ZEN. Zwraca false, jesli go nie ma
// (nie kazdy testowy/czesciowy swiat go zawiera).
static bool findStartPointFromZen(const std::string& path, glm::vec3& outPos) {
  try {
    auto reader = zenkit::Read::from(path);
    zenkit::World world;
    world.load(reader.get());
    bool found = false;
    for(auto& vob : world.world_vobs) {
      walkVobsForStartPoint(vob, found, outPos);
      if(found) break;
      }
    if(found)
      printf("Znaleziono zCVobStartpoint: (%.1f, %.1f, %.1f)\n", outPos.x, outPos.y, outPos.z);
    else
      printf("Brak zCVobStartpoint w tym pliku - kamera wystartuje przy centroidzie swiatel.\n");
    return found;
    }
  catch(const std::exception& e) {
    fprintf(stderr, "Nie udalo sie sprawdzic startpointu w %s: %s\n", path.c_str(), e.what());
    return false;
    }
  }

static void addQuad(std::vector<Vertex>& out,
                     glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                     glm::vec3 n) {
  out.push_back({a,n}); out.push_back({b,n}); out.push_back({c,n});
  out.push_back({a,n}); out.push_back({c,n}); out.push_back({d,n});
  }

static void addBox(std::vector<Vertex>& out, glm::vec3 c, glm::vec3 half) {
  glm::vec3 p[8] = {
    c+glm::vec3(-half.x,-half.y,-half.z), c+glm::vec3( half.x,-half.y,-half.z),
    c+glm::vec3( half.x, half.y,-half.z), c+glm::vec3(-half.x, half.y,-half.z),
    c+glm::vec3(-half.x,-half.y, half.z), c+glm::vec3( half.x,-half.y, half.z),
    c+glm::vec3( half.x, half.y, half.z), c+glm::vec3(-half.x, half.y, half.z),
    };
  addQuad(out, p[0],p[1],p[2],p[3], { 0, 0,-1}); // przod
  addQuad(out, p[5],p[4],p[7],p[6], { 0, 0, 1}); // tyl
  addQuad(out, p[4],p[0],p[3],p[7], {-1, 0, 0}); // lewo
  addQuad(out, p[1],p[5],p[6],p[2], { 1, 0, 0}); // prawo
  addQuad(out, p[3],p[2],p[6],p[7], { 0, 1, 0}); // gora
  addQuad(out, p[4],p[5],p[1],p[0], { 0,-1, 0}); // dol
  }

static void addTriangle(
    std::vector<Vertex>& out,
    glm::vec3 a,
    glm::vec3 b,
    glm::vec3 c,
    glm::vec3 normal)
{
    out.push_back({a, normal});
    out.push_back({b, normal});
    out.push_back({c, normal});
}

static void addTriangle(
    std::vector<Vertex>& out,
    glm::vec3 center,
    float width,
    float height,
    glm::vec3 normal)
{
    glm::vec3 a = center + glm::vec3(-width * 0.5f, -height * 0.5f, 0.f);
    glm::vec3 b = center + glm::vec3( width * 0.5f, -height * 0.5f, 0.f);
    glm::vec3 c = center + glm::vec3(0.f, height * 0.5f, 0.f);

    out.push_back({a, normal});
    out.push_back({b, normal});
    out.push_back({c, normal});
}


// pokoj: podloga + 4 sciany, skala w cm (jak w Gothicu) - promien ~1500,
// wysokosc scian 400, zeby bylo miejsce na testowanie duzych Range (AURA=3000
// wystaje poza pokoj celowo - i o to chodzi, zeby zobaczyc peine gasniecie)
static std::vector<Vertex> buildRoom()
{
    std::vector<Vertex> v;

    float R = 1500.f;
    float H = 400.f;
    float T = 20.f;

    // --------------------------------------------------
    // PODŁOGA
    // --------------------------------------------------
    addQuad(
        v,
        {-R, 0.f, -R},
        { R, 0.f, -R},
        { R, 0.f,  R},
        {-R, 0.f,  R},
        {0.f, 1.f, 0.f}
    );

    // --------------------------------------------------
    // SUFIT
    // --------------------------------------------------
    float ceilingR = R / 5.f;

    addQuad(
        v,
        {-ceilingR, H, -ceilingR},
        { ceilingR, H, -ceilingR},
        { ceilingR, H,  ceilingR},
        {-ceilingR, H,  ceilingR},
        {0.f, -1.f, 0.f}
    );

    // --------------------------------------------------
    // JEDNA ŚCIANA ZA ŚWIATŁEM
    // Z = -500
    // --------------------------------------------------
    addBox(
        v,
        {0.f, H * 0.5f, -500.f},
        {600.f, H * 0.5f, T}
    );

    // --------------------------------------------------
    // TRÓJKĄT JESZCZE DALEJ
    // Z = -700
    // --------------------------------------------------
    addTriangle(
        v,
        {-500.f, 0.f, -700.f},
        { 500.f, 0.f, -700.f},
        {   0.f, H,   -700.f},
        {0.f, 0.f, 1.f}
    );

    // ==================================================
    // OBIEKTY DALEKO OD ŚWIATŁA - ok. 10x dalej
    // ==================================================

    // Duża ściana w tle
    addBox(
        v,
        {0.f, 200.f, -5000.f},
        {800.f, 200.f, 20.f}
    );

    // Wąska wysoka kolumna po lewej
    addBox(
        v,
        {-1000.f, 300.f, -4500.f},
        {100.f, 300.f, 100.f}
    );

    // Wąska kolumna po prawej
    addBox(
        v,
        {1000.f, 200.f, -5500.f},
        {150.f, 200.f, 150.f}
    );

    // Duży trójkąt jeszcze dalej
    addTriangle(
        v,
        {-500.f, 0.f, -5000.f},
        { 500.f, 0.f, -5000.f},
        {   0.f, 800.f, -5000.f},
        {0.f, 0.f, 1.f}
    );


    return v;
}


// plaska podloga dopasowana do bounding-boxa realnych swiatel wczytanych z .ZEN
// (nie mamy prawdziwej geometrii poziomu - to tylko plaszczyzna odniesienia,
// zeby widziec padanie/gasniecie swiatla w relacji do rzeczywistych pozycji)
static std::vector<Vertex> buildFloorForLights(const std::vector<LoadedLight>& lights, float& outY)
{
  float minX=1e9f, maxX=-1e9f, minZ=1e9f, maxZ=-1e9f, minY=1e9f;
  for(auto& l : lights) {
    minX = std::min(minX, l.pos.x); maxX = std::max(maxX, l.pos.x);
    minZ = std::min(minZ, l.pos.z); maxZ = std::max(maxZ, l.pos.z);
    minY = std::min(minY, l.pos.y);
    }
  float margin = 500.f;
  minX -= margin; maxX += margin;
  minZ -= margin; maxZ += margin;
  outY = minY - 50.f; // podloga tuz pod najnizszym swiatlem

  std::vector<Vertex> v;
  addQuad(v, {minX,outY,minZ}, {maxX,outY,minZ}, {maxX,outY,maxZ}, {minX,outY,maxZ}, {0,1,0});
  return v;
}

static std::vector<Vertex> buildFogPoints(glm::vec3 bmin, glm::vec3 bmax, int count)
{
  std::vector<Vertex> v;
  v.reserve(count);
  std::mt19937 rng(1337);
  std::uniform_real_distribution<float> dx(bmin.x, bmax.x);
  std::uniform_real_distribution<float> dy(bmin.y, bmax.y);
  std::uniform_real_distribution<float> dz(bmin.z, bmax.z);
  for(int i=0;i<count;i++)
    v.push_back({ {dx(rng), dy(rng), dz(rng)}, {0,0,1} });
  return v;
}



static Camera g_cam;
static bool   g_keys[512] = {};
static double g_lastX = 400, g_lastY = 300;
static bool   g_firstMouse = true;
static bool   g_mouseCaptured = true;

static int   g_formulaMode    = 1; // 0=linia, 1=obecna
static int   g_lightcorrection = 0; // 0=brak, 1=obecna
static int   g_tonemap        = 1; // wlaczony domyslnie - tak jak w OpenGothic
static float g_lightIntensity = 1.f;
static bool  g_fogEnabled    = false;
static float g_fogDensity    = 1.0f;
static float g_fogPointSize  = 4.f;

static void keyCallback(GLFWwindow* w, int key, int, int action, int)
{
  if(action==GLFW_PRESS || action==GLFW_RELEASE)
    g_keys[key] = (action==GLFW_PRESS);

  if(action!=GLFW_PRESS) return;

  if(key==GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
  if(key==GLFW_KEY_M) g_formulaMode ^= 1;
  if(key ==GLFW_KEY_N) g_lightcorrection ^= 1;
  if(key==GLFW_KEY_T) g_tonemap ^= 1;
  if(key>=GLFW_KEY_1 && key<=GLFW_KEY_6) {
    int idx = key-GLFW_KEY_1;
    if(idx < int(sizeof(PRESETS)/sizeof(PRESETS[0]))) {
      g_presetIdx = idx;
      }
    }
  if(key==GLFW_KEY_LEFT_BRACKET)  g_lightIntensity = std::max(0.f, g_lightIntensity-0.02f);
  if(key==GLFW_KEY_RIGHT_BRACKET) g_lightIntensity += 0.02f;
  if(key==GLFW_KEY_F) g_fogEnabled ^= 1;
  if(key==GLFW_KEY_O) g_fogDensity = std::max(0.f, g_fogDensity-0.1f);
  if(key==GLFW_KEY_P) g_fogDensity += 0.1f;
}

static void cursorCallback(GLFWwindow*, double x, double y) {
  if(!g_mouseCaptured) return;
  if(g_firstMouse) { g_lastX=x; g_lastY=y; g_firstMouse=false; }
  double dx = x-g_lastX, dy = g_lastY-y;
  g_lastX=x; g_lastY=y;
  const float sens = 0.12f;
  g_cam.yaw   += float(dx)*sens;
  g_cam.pitch += float(dy)*sens;
  g_cam.pitch  = std::clamp(g_cam.pitch, -89.f, 89.f);
  }

static void deleteVao(GLuint& vao) 
{
  if(vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
  // uwaga: to nie usuwa powiazanego VBO - jesli chcesz w pelni posprzatac,
  // makeVao powinno zwracac pare {vao, vbo} zamiast samego vao.
}

static const float g_fogTargetDensity =  0.0002f; // punktow na jednostke objetosci 

static int fogPointCountForRange(float range)
{
  float side = 4.f * range; // bmax-bmin w kazdej osi przy mnozniku 2*range
  float volume = side * side * side;
  int count = int(volume * g_fogTargetDensity);
  return std::clamp(count, 200, 2000000); // bezpiecznik gorny/dolny
}


int main(int argc, char** argv) {
  std::vector<LoadedLight> worldLights;
  bool worldMode = false;
  if(argc>1)
  {
    worldLights = loadLightsFromZen(argv[1]);
    worldMode = !worldLights.empty();
    if(argc>1 && !worldMode)
      fprintf(stderr, "Brak swiatel dynamicznych w %s - przechodze do trybu demo.\n", argv[1]);
  }

  if(!worldMode)
  {
    // brak swiatla z ZEN - dodajemy jeden wpis reprezentujacy swiatlo demo.
    // Jego pos/range/color/rSource beda aktualizowane co klatke z aktualnego
    // presetu (klawisze 1-6, M, -/=), wiec tu wpisujemy tylko wartosci startowe.
    LoadedLight demo;
    demo.pos     = {0.f, 120.f, 0.f};
    demo.range   = PRESETS[g_presetIdx].range;
    demo.color   = PRESETS[g_presetIdx].color;
    demo.preset  = PRESETS[g_presetIdx].name;
    worldLights.push_back(demo);
  }


  if(!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow* win = glfwCreateWindow(1280, 720, "lighttest", nullptr, nullptr);
  if(!win) { fprintf(stderr, "glfwCreateWindow failed\n"); glfwTerminate(); return 1; }
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);
  glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetKeyCallback(win, keyCallback);
  glfwSetCursorPosCallback(win, cursorCallback);

  glEnable(GL_DEPTH_TEST);

  GLuint vs = compileShader(GL_VERTEX_SHADER, VERT_SRC);
  GLuint fs = compileShader(GL_FRAGMENT_SHADER, FRAG_SRC);
  GLuint prog = linkProgram(vs, fs);
  glDeleteShader(vs);
  glDeleteShader(fs);

  auto makeVao = [](const std::vector<Vertex>& verts) {
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,normal));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return vao;
    };

  GLuint floorVao;
  size_t floorVertCount;
  glm::vec3 camStart;

  if(worldMode) //to jest false jeśli nie na świateł w pliku zen
  {
    std::vector<Vertex> floorVerts = loadWorldMeshFromZen(argv[1]);
    if(floorVerts.empty()) {
      // fallback: brak/pusta siatka - plaska podloga dopasowana do swiatel
      float floorY = 0.f;
      floorVerts = buildFloorForLights(worldLights, floorY);
      }
    floorVao = makeVao(floorVerts);
    floorVertCount = floorVerts.size();

    glm::vec3 centroid(0.f);
    for(auto& l : worldLights) centroid += l.pos;
    centroid /= float(worldLights.size());

    glm::vec3 startPos;
    bool hasStart = findStartPointFromZen(argv[1], startPos);
    if(hasStart) {
      camStart   = startPos + glm::vec3(0.f, 80.f, 0.f); // +80 = orientacyjna wysokosc oczu nad stopami
      g_cam.pos  = camStart;
      }
    else {
      camStart  = centroid + glm::vec3(0.f, 400.f, 800.f);
      g_cam.pos = camStart;
      }
    glm::vec3 dir = glm::normalize(centroid-camStart);
    g_cam.pitch = glm::degrees(asin(dir.y));
    g_cam.yaw   = glm::degrees(atan2(dir.z, dir.x));
  }
  else //brak świateł w pliku ZEN lub nawet pliku dodajmey nasz sztuczny prosty świat
  {
    std::vector<Vertex> roomVerts = buildRoom();
    floorVao = makeVao(roomVerts);
    floorVertCount = roomVerts.size();
  }

  std::vector<Vertex> markerVerts;
  addBox(markerVerts, {0,0,0}, {15,15,15});
  GLuint markerVao = makeVao(markerVerts);

  std::vector<std::vector<Vertex>> fogPerLight;
  std::vector<GLuint> fogVaoPerLight;
  std::vector<size_t> fogCountPerLight;

  for(auto& l : worldLights)
  {
    glm::vec3 bmin = l.pos - glm::vec3(2*l.range);
    glm::vec3 bmax = l.pos + glm::vec3(2*l.range);
    int count = fogPointCountForRange(l.range);
    auto pts = buildFogPoints(bmin, bmax, count);
    fogVaoPerLight.push_back(makeVao(pts));
    fogCountPerLight.push_back(pts.size());
  }
  glEnable(GL_PROGRAM_POINT_SIZE);

  TextRenderer text;
  text.init();

  glm::vec3 demoLightPos = {0.f, 120.f, 0.f}; // swiatlo na srodku pokoju (tryb demo)

  int lastPresetIdx = g_presetIdx;

  double lastTime = glfwGetTime();
  while(!glfwWindowShouldClose(win)) {
    double now = glfwGetTime();
    float dt = float(now-lastTime);
    lastTime = now;

    glfwPollEvents();

    float speed = g_cam.speed * dt * (g_keys[GLFW_KEY_LEFT_SHIFT] ? 3.f : 1.f);
    if(g_keys[GLFW_KEY_W]) g_cam.pos += g_cam.front()*speed;
    if(g_keys[GLFW_KEY_S]) g_cam.pos -= g_cam.front()*speed;
    if(g_keys[GLFW_KEY_A]) g_cam.pos -= g_cam.right()*speed;
    if(g_keys[GLFW_KEY_D]) g_cam.pos += g_cam.right()*speed;
    if(g_keys[GLFW_KEY_SPACE])       g_cam.pos.y += speed;
    if(g_keys[GLFW_KEY_LEFT_CONTROL]) g_cam.pos.y -= speed;

    int fbw, fbh;
    glfwGetFramebufferSize(win, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);
    glClearColor(0.02f, 0.02f, 0.03f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 proj = glm::perspective(glm::radians(70.f), float(fbw)/float(fbh), 1.f, 8000.f);
    glm::mat4 view = g_cam.view();

    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(prog,"uProj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1f(glGetUniformLocation(prog,"uLightIntensity"), g_lightIntensity);
    glUniform1i(glGetUniformLocation(prog,"uFormulaMode"), g_formulaMode);
    glUniform1i(glGetUniformLocation(prog,"uTonemap"), g_tonemap);
    glUniform3f(glGetUniformLocation(prog,"uAlbedo"), 0.6f, 0.6f, 0.62f);
    glUniform1i(glGetUniformLocation(prog,"uIsMarker"), 0);

    glm::mat4 model(1.f);
    glUniformMatrix4fv(glGetUniformLocation(prog,"uModel"), 1, GL_FALSE, glm::value_ptr(model));

    // przebieg 1: sam ambient, bez blendingu (ustawia tez depth buffer podlogi)
    //czyli rysujemy siatke pomieszczenia stałym kolorem
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glUniform1i(glGetUniformLocation(prog,"uAmbientOnly"), 1);
    glBindVertexArray(floorVao);
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(floorVertCount));

    // przebiegi 2..N: po jednym na kazde swiatlo, addytywnie (GL_ONE,GL_ONE),
    // bez zapisu do depth buffer (ta sama geometria, ten sam depth co wyzej)
    //czyli teraz rysujemy kolroki od źródeł światła oraz boxy w miejscach źródeł światła
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glUniform1i(glGetUniformLocation(prog,"uAmbientOnly"), 0);
    glUniform1f(glGetUniformLocation(prog,"uPointSizeBase"), g_fogPointSize);
    glUniform1f(glGetUniformLocation(prog,"uFogDensity"), g_fogDensity);



    // //rYSUJEMY SWIATŁA

    if(!worldMode)
    {
      const Preset& pr = PRESETS[g_presetIdx];
      worldLights.back().pos     = demoLightPos;
      worldLights.back().range   = pr.range;
      worldLights.back().color   = pr.color;
      worldLights.back().preset  = pr.name;

      if(g_presetIdx != lastPresetIdx)
      {
        lastPresetIdx = g_presetIdx;
        glm::vec3 bmin = worldLights.back().pos - glm::vec3(2.f * pr.range);
        glm::vec3 bmax = worldLights.back().pos + glm::vec3(2.f * pr.range);
        int count = fogPointCountForRange(pr.range);
        auto pts = buildFogPoints(bmin, bmax, count);

        deleteVao(fogVaoPerLight.back());
        fogVaoPerLight.back()   = makeVao(pts);
        fogCountPerLight.back() = pts.size();
        // Uwaga: stary VAO/VBO wyciekaja (nie sa zwalniane) - patrz punkt 6 nizej
      }

    }


    for(size_t i = 0; i < worldLights.size(); ++i) 
    {
      {
        const auto& l = worldLights[i];
        glUniform3fv(glGetUniformLocation(prog,"uLightPos"), 1, glm::value_ptr(l.pos));
        glUniform3fv(glGetUniformLocation(prog,"uLightColor"), 1, glm::value_ptr(l.color));
        if(g_formulaMode == 0 || g_lightcorrection == 0)
        {
          glUniform1f(glGetUniformLocation(prog,"uRange"), l.range);
        }
        else
        {
          float effectiveRange = correctedRange(l.range);
          glUniform1f(glGetUniformLocation(prog,"uRange"), effectiveRange);
        }


        glUniform1i(glGetUniformLocation(prog,"uIsFog"), 0);
        glBindVertexArray(floorVao);
        glDrawArrays(GL_TRIANGLES, 0, GLsizei(floorVertCount));


        if(g_fogEnabled)
        {
          glUniform1i(glGetUniformLocation(prog,"uIsFog"), 1);
          glBindVertexArray(fogVaoPerLight[i]);            // <- TYLKO chmura NALEZACA do tego swiatla
          glDrawArrays(GL_POINTS, 0, GLsizei(fogCountPerLight[i]));

        }

#ifdef LIGHTTEST_GL_DEBUG
        GLenum err;
        while((err = glGetError()) != GL_NO_ERROR)
        {
          fprintf(stderr, "GL error po rysowaniu fog: 0x%x\n", err);
        }
#endif
      }


      glUniform1i(glGetUniformLocation(prog,"uIsFog"), 0); // reset przed markerami
    }



    // znaczniki swiatel - opaque, z powrotem normalny depth test/write
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glUniform1i(glGetUniformLocation(prog,"uIsMarker"), 1);
    glBindVertexArray(markerVao);

    float hudDist = 0.f, hudRange = 1.f;
    const char* hudPreset = "demo";

    float nearestDist = 1e9f;
    for(auto& l : worldLights) {
      glUniform3fv(glGetUniformLocation(prog,"uLightColor"), 1, glm::value_ptr(l.color));
      glm::mat4 markerModel = glm::translate(glm::mat4(1.f), l.pos);
      glUniformMatrix4fv(glGetUniformLocation(prog,"uModel"), 1, GL_FALSE, glm::value_ptr(markerModel));
      glDrawArrays(GL_TRIANGLES, 0, GLsizei(markerVerts.size()));

      float d = glm::length(l.pos-g_cam.pos);
      if(d<nearestDist) {
        nearestDist = d;
        hudDist = d; hudRange = l.range;
        hudPreset = l.preset.empty() ? "(brak nazwy)" : l.preset.c_str();
        }
      }
    


    drawHud(text, fbw, fbh, g_cam, hudPreset, hudRange, hudDist, g_formulaMode, g_lightcorrection, g_tonemap, g_lightIntensity, g_fogEnabled, g_fogDensity);

    glfwSwapBuffers(win);

    char title[700];
    if(worldMode) {
      snprintf(title, sizeof(title),
        "lighttest [SWIAT: %zu swiatel] | kamera=(%.1f, %.1f, %.1f) | najblizsze: %s Range=%.0f d=%.1f (d/R=%.1f%%) | "
        "formula=%s tonemap=%s LightIntensity=%.3f | [M] formula [T] tonemap [/]] intensity",
        worldLights.size(), g_cam.pos.x, g_cam.pos.y, g_cam.pos.z,
        hudPreset, hudRange, hudDist, 100.f*hudDist/hudRange,
        g_formulaMode==0 ? "LINIA" : "OBECNA",
        g_tonemap ? "ON" : "OFF",
        g_lightIntensity);
      }
    else {
      snprintf(title, sizeof(title),
        "lighttest [DEMO] | kamera=(%.1f, %.1f, %.1f) | preset=%s Range=%.0f d=%.1f (d/R=%.1f%%) | "
        "formula=%s tonemap=%s LightIntensity=%.3f | [1-6] preset [M] formula [T] tonemap [/]] intensity ",
        g_cam.pos.x, g_cam.pos.y, g_cam.pos.z,
        hudPreset, hudRange, hudDist, 100.f*hudDist/hudRange,
        g_formulaMode==0 ? "LINIA" : "OBECNA",
        g_tonemap ? "ON" : "OFF",
        g_lightIntensity);
      }
    glfwSetWindowTitle(win, title);
    }

  glfwTerminate();
  return 0;
  }
