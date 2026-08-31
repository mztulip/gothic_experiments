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
#include <chrono>

#include "info_render.hpp"
#include "camera.hpp"
#include "light_correction.hpp"
#include "shader_utils.hpp"
#include "light.hpp"
#include "gbuffer.hpp"
#include "zen_loader.hpp"
#include "shaders.hpp"
#include "texture_loader.hpp"
#include "fog_buffer.hpp"
#include "geometry_room.hpp"

static int g_presetIdx = 1; // start na FIRE - to ten najbardziej sporny


static std::mt19937 rng(1337);

static std::vector<Vertex> buildFogPoints(glm::vec3 bmin, glm::vec3 bmax, int count)
{
  std::vector<Vertex> v;
  v.reserve(count);

  std::uniform_real_distribution<float> dx(bmin.x, bmax.x);
  std::uniform_real_distribution<float> dy(bmin.y, bmax.y);
  std::uniform_real_distribution<float> dz(bmin.z, bmax.z);
  for(int i=0;i<count;i++)
    v.push_back({ {dx(rng), dy(rng), dz(rng)}, {0,0,1} });
  return v;
}


static GBuffer g_gbuf;

static Camera g_cam;
static bool   g_keys[512] = {};
static double g_lastX = 400, g_lastY = 300;
static bool   g_firstMouse = true;
static bool   g_mouseCaptured = true;

static int   g_formulaMode    = 1; // 0=linia, 1=obecna
static int   g_lightcorrection = 1; // 0=brak, 1=obecna
static int   g_tonemap        = 1; // wlaczony domyslnie - tak jak w OpenGothic
static float g_lightIntensity = 1.f;
static bool  g_fogEnabled    = false;
static float g_fogDensity    = 1.0f;
static bool  g_texturesEnabled = true;
static float g_fogPointSize  = 4.f;
static bool g_lightCorrectionChanged = false;


static void keyCallback(GLFWwindow* w, int key, int, int action, int)
{
  if(action==GLFW_PRESS || action==GLFW_RELEASE)
    g_keys[key] = (action==GLFW_PRESS);

  if(action!=GLFW_PRESS) return;

  if(key==GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
  if(key==GLFW_KEY_Y) g_texturesEnabled ^= 1;
  if(key==GLFW_KEY_M) g_formulaMode ^= 1;
  if(key == GLFW_KEY_N)
  {
      g_lightcorrection ^= 1;
      g_lightCorrectionChanged = true;
  }

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


static const float g_fogTargetDensity =  0.0002f; // punktow na jednostke objetosci 

static int fogPointCountForRange(float range)
{
  float side = 4.f * range; // bmax-bmin w kazdej osi przy mnozniku 2*range
  float volume = side * side * side;
  int count = int(volume * g_fogTargetDensity);
  return std::clamp(count, 200, 2000000); // bezpiecznik gorny/dolny
}

static float effectiveLightRange(const LoadedLight& l)
{
    return (g_formulaMode == 0 || g_lightcorrection == 0)
        ? l.range
        : correctedRange(l.range);
}



int main(int argc, char** argv)
{
  std::string zenPath;
  TextureSource texSource = TextureSource::Both;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--mydata-only" || a == "--only-mydata") {
      texSource = TextureSource::MyDataOnly;
    } else if (a == "--gothic-only" || a == "--only-gothic") {
      texSource = TextureSource::GothicOnly;
    } else if (zenPath.empty()) {
      zenPath = a; // pierwszy "zwykly" argument to sciezka do .ZEN
    }
  }

  std::vector<LoadedLight> worldLights;
  bool worldMode = false;
  if(argc>1)
  {
    worldLights = loadLightsFromZen(argv[1]);
    worldMode = !worldLights.empty();
    if(argc>1 && !worldMode)
      fprintf(stderr, "Brak swiatel dynamicznych w %s - przechodze do trybu demo.\n", argv[1]);
  }

  std::vector<LoadedVob> worldVobs;

  if (worldMode)
  {
      worldVobs = loadVobsFromZen(zenPath);
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

  TextureCache texCache;
  texCache.indexDirectory("/home/mz/.wine/drive_c/Program Files (x86)/JoWood/Gothic II/", texSource);
  std::vector<SubMesh> worldSubMeshes;

  glm::vec3 camStart;

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
  std::string fragFullSrc = buildFragSource(FRAG_SRC);
  GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragFullSrc.c_str());
  GLuint prog = linkProgram(vs, fs);
  glDeleteShader(vs);
  glDeleteShader(fs);

  GLuint geomVs = compileShader(GL_VERTEX_SHADER, GEOM_VERT_SRC);
  GLuint geomFs = compileShader(GL_FRAGMENT_SHADER, GEOM_FRAG_SRC); // bez zmian, nie uzywa wspolnej logiki
  GLuint geomProg = linkProgram(geomVs, geomFs);
  glDeleteShader(geomVs);
  glDeleteShader(geomFs);

  GLuint lightVs = compileShader(GL_VERTEX_SHADER, LIGHT_VERT_SRC);
  std::string lightFragFullSrc = buildFragSource(LIGHT_FRAG_SRC);
  GLuint lightFs = compileShader(GL_FRAGMENT_SHADER, lightFragFullSrc.c_str());
  GLuint lightProg = linkProgram(lightVs, lightFs);
  glDeleteShader(lightVs);
  glDeleteShader(lightFs);

  GLint locLightPos   = glGetUniformLocation(lightProg, "uLightPos");
  GLint locLightColor = glGetUniformLocation(lightProg, "uLightColor");
  GLint locRange      = glGetUniformLocation(lightProg, "uRange");


  GLuint quadVao = makeFullscreenQuad();

// gbufor inicjujemy dopiero w petli (znamy tam fbw/fbh), patrz ensureSize() nizej

auto makeVao = [](const std::vector<Vertex>& verts) {
    GLuint vao, vbo;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        verts.size() * sizeof(Vertex),
        verts.data(),
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, pos)
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, normal)
    );
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, uv)
    );
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    return vao;
};


  auto t0 = std::chrono::high_resolution_clock::now();
  auto t1 = std::chrono::high_resolution_clock::now();
  auto t2 = std::chrono::high_resolution_clock::now();
  auto t3 = std::chrono::high_resolution_clock::now();
  if(worldMode) //to jest false jeśli nie na świateł w pliku zen
  {
    worldSubMeshes = loadWorldSubMeshesFromZen(zenPath, texCache);

    t1 = std::chrono::high_resolution_clock::now();
    printf("[LOG] Wczytanie ZEN z dysku: %.2f ms\n",
           std::chrono::duration<float, std::milli>(t1 - t0).count());

    printf("[LOG] Utworzono %zu podsiatek świata\n",
           worldSubMeshes.size());

    t2 = std::chrono::high_resolution_clock::now();

    glm::vec3 centroid(0.f);
    for(auto& l : worldLights) centroid += l.pos;
    centroid /= float(worldLights.size());

    glm::vec3 startPos;
    bool hasStart = findStartPointFromZen(zenPath, startPos);
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
    //  Tworzenie sztucznego pokoju w trybie demo w identycznej strukturze SubMesh
    std::vector<Vertex> roomVerts = buildRoom();
    SubMesh demoMesh;
    demoMesh.vertexCount = roomVerts.size();
    demoMesh.fallbackColor = glm::vec3(0.6f, 0.6f, 0.62f);
    demoMesh.texture.valid = false; // Brak tekstury - shader użyje fallbackColor

    demoMesh.vao = makeVao(roomVerts);
    worldSubMeshes.push_back(demoMesh);
  }

  std::vector<Vertex> markerVerts;
  //Boxy dla źródeł światła
  addBox(markerVerts, {0,0,0}, {10,10,10});
  GLuint markerVao = makeVao(markerVerts);

  std::vector<Vertex> objectMarkerVerts;
  addBox(
      objectMarkerVerts,
      {0, 0, 0},
      {5, 5, 5}
  );

  GLuint objectMarkerVao = makeVao(objectMarkerVerts);


  std::vector<size_t> visibleLightIndices;
  visibleLightIndices.reserve(worldLights.size());

  // Zamiast generować wszystko na starcie, tworzymy wektory o odpowiednim rozmiarze, ale puste/nie zainicjalizowane
  // std::vector<GLuint> fogVaoPerLight(worldLights.size(), 0);
  // std::vector<size_t> fogCountPerLight(worldLights.size(), 0);
  // std::vector<bool>   fogGeneratedPerLight(worldLights.size(), false);
  std::vector<FogBuffer> fogPerLight(worldLights.size());


  t3 = std::chrono::high_resolution_clock::now();
  printf("[LOG] Pominięto wstępne generowanie mgły – włączono tryb dynamiczny (leniwy).\n");

  // t3 = std::chrono::high_resolution_clock::now();
  // printf("[LOG] Czas generowania mgły: %.2f ms\n", 
  //       std::chrono::duration<float, std::milli>(t3 - t2).count());
  glEnable(GL_PROGRAM_POINT_SIZE);

  TextRenderer text;
  text.init();

  glm::vec3 demoLightPos = {0.f, 120.f, 0.f}; // swiatlo na srodku pokoju (tryb demo)

  double lastTime = glfwGetTime();
  float  g_fpsSmoothed = 0.f;

  while(!glfwWindowShouldClose(win)) 
  {
    double now = glfwGetTime();
    float dt = float(now-lastTime);
    lastTime = now;

    // wygladzanie EMA (exponential moving average) - stabilny odczyt zamiast szumu klatka-do-klatki
    if(dt > 0.0001f) 
    {
      float instantFps = 1.f / dt;
      const float smoothing = 0.9f; // wiekszy = wolniej reaguje, stabilniejszy odczyt
      g_fpsSmoothed = g_fpsSmoothed <= 0.f
        ? instantFps
        : g_fpsSmoothed * smoothing + instantFps * (1.f - smoothing);
    }

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
    g_gbuf.ensureSize(fbw, fbh); // realokuje tekstury tylko gdy zmienil sie rozmiar okna
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


    glUniform1i(glGetUniformLocation(prog,"uAmbientOnly"), 0);
    glUniform1f(glGetUniformLocation(prog,"uPointSizeBase"), g_fogPointSize);
    glUniform1f(glGetUniformLocation(prog,"uFogDensity"), g_fogDensity);

    // ============================================================
    // PASS 1: geometria swiata - RAZ, do G-bufora
    // ============================================================
    glBindFramebuffer(GL_FRAMEBUFFER, g_gbuf.fbo);
    glViewport(0, 0, g_gbuf.w, g_gbuf.h);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(geomProg);
    glUniformMatrix4fv(glGetUniformLocation(geomProg,"uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(geomProg,"uProj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(geomProg,"uModel"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.f)));
    
    GLint locHasTex = glGetUniformLocation(geomProg, "uHasTexture");
    GLint locAlbedo = glGetUniformLocation(geomProg, "uAlbedo");
    glUniform1i(glGetUniformLocation(geomProg, "uTexture"), 0);

    // glBindVertexArray(floorVao);
    // glDrawArrays(GL_TRIANGLES, 0, GLsizei(floorVertCount)); // <-- TYLKO RAZ na klatke

    for (const auto& sm : worldSubMeshes)
    {
        const bool useTexture = g_texturesEnabled && sm.texture.valid;
        if (useTexture)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sm.texture.id);
            glUniform1i(locHasTex, 1);
        }
        else
        {
            glUniform1i(locHasTex, 0);
            glUniform3fv(locAlbedo, 1, glm::value_ptr(sm.fallbackColor));
        }

        glBindVertexArray(sm.vao);
        glDrawArrays(GL_TRIANGLES, 0, GLsizei(sm.vertexCount));
    }
    glBindVertexArray(0);

    // skopiuj glebie do domyslnego framebuffera, zeby markery/mgla mialy poprawny depth test
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_gbuf.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, g_gbuf.w, g_gbuf.h, 0, 0, fbw, fbh, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    // ============================================================
    // PASS 2: ambient na ekran (fullscreen quad, brak blendingu)
    // ============================================================
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbw, fbh);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);

    glUseProgram(lightProg);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, g_gbuf.texAlbedo);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, g_gbuf.texNormal);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, g_gbuf.texWorldPos);
    glUniform1i(glGetUniformLocation(lightProg,"uGAlbedo"), 0);
    glUniform1i(glGetUniformLocation(lightProg,"uGNormal"), 1);
    glUniform1i(glGetUniformLocation(lightProg,"uGWorldPos"), 2);
    glUniform1i(glGetUniformLocation(lightProg,"uAmbientOnly"), 1);
    glBindVertexArray(quadVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // ============================================================
    // AKTUALIZACJA ZMIAN ZALEŻNYCH OD LIGHT CORRECTION
    // ============================================================

    if(g_lightCorrectionChanged)
    {
        for(auto& fog : fogPerLight)
            deleteFogBuffer(fog);

        g_lightCorrectionChanged = false;
    }


    // ============================================================
    // AKTUALIZACJA ŚWIATŁA DEMO
    // ============================================================
    

    if(!worldMode)
    {
      const Preset& pr = PRESETS[g_presetIdx];

      worldLights.back().pos    = demoLightPos;
      worldLights.back().range  = pr.range;
      worldLights.back().color  = pr.color;
      worldLights.back().preset = pr.name;
    }

    // ============================================================
    // WYBÓR ŚWIATEŁ WIDOCZNYCH DLA KAMERY + LENIWA GENERACJA FOG
    // ============================================================

    visibleLightIndices.clear();

    for(size_t i = 0; i < worldLights.size(); ++i)
    {
        const auto& l = worldLights[i];

        // Faktyczny Range używany przez renderer.
        const float range = effectiveLightRange(l);

        const float distToCam =
            glm::length(l.pos - g_cam.pos);

        if(distToCam > range * 5.0f)
            continue;

        visibleLightIndices.push_back(i);

        // --------------------------------------------------------
        // Fog generujemy tylko wtedy, gdy jest potrzebny.
        // --------------------------------------------------------

        if(g_fogEnabled)
        {
            FogBuffer& fog = fogPerLight[i];

            const bool rangeChanged =
                std::abs(fog.range - range) > 0.01f;

            if(!fog.generated || rangeChanged)
            {
                glm::vec3 bmin =
                    l.pos - glm::vec3(2.f * range);

                glm::vec3 bmax =
                    l.pos + glm::vec3(2.f * range);

                const int count =
                    fogPointCountForRange(range);

                auto pts =
                    buildFogPoints(bmin, bmax, count);

                deleteFogBuffer(fog);

                fog.vao = makeVao(pts);
                fog.count = pts.size();
                fog.range = range;
                fog.generated = true;
            }
        }
    }

    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUniform1i(glGetUniformLocation(lightProg,"uAmbientOnly"), 0);
    glUniform1i(glGetUniformLocation(lightProg,"uFormulaMode"), g_formulaMode);
    glUniform1f(glGetUniformLocation(lightProg,"uLightIntensity"), g_lightIntensity);


    for(size_t i : visibleLightIndices)
    {
        const auto& l = worldLights[i];

        float range = effectiveLightRange(l);

        int x0, y0, x1, y1;

        if(!lightScissorRect(
                l.pos,
                range,
                view,
                proj,
                fbw,
                fbh,
                x0, y0, x1, y1))
        {
            continue;
        }

        glScissor(x0, y0, x1 - x0, y1 - y0);

        glUniform3fv(
            locLightPos,
            1,
            glm::value_ptr(l.pos)
        );

        glUniform3fv(
            locLightColor,
            1,
            glm::value_ptr(l.color)
        );

        glUniform1f(locRange, range);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }


    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    glUseProgram(prog);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);


    for(size_t i : visibleLightIndices)
    {
        const auto& l = worldLights[i];

        glUniform3fv(
            glGetUniformLocation(prog, "uLightPos"),
            1,
            glm::value_ptr(l.pos)
        );

        glUniform3fv(
            glGetUniformLocation(prog, "uLightColor"),
            1,
            glm::value_ptr(l.color)
        );

        float range = effectiveLightRange(l);

        glUniform1f(
            glGetUniformLocation(prog, "uRange"),
            range
        );

        if(g_fogEnabled)
        {
            glUniform1i(glGetUniformLocation(prog, "uIsFog"), 1);

            const FogBuffer& fog = fogPerLight[i];

            if(fog.generated && fog.vao != 0 && fog.count > 0)
            {
                glBindVertexArray(fog.vao);

                glDrawArrays(
                    GL_POINTS,
                    0,
                    GLsizei(fog.count)
                );
            }

        }

#ifdef LIGHTTEST_GL_DEBUG
        GLenum err;
        while((err = glGetError()) != GL_NO_ERROR)
        {
          fprintf(stderr, "GL error po rysowaniu fog: 0x%x\n", err);
        }
#endif
    }

    glUniform1i(glGetUniformLocation(prog, "uIsFog"), 0);

    ///////////////////////////////////////////////////
    // znaczniki swiatel 
    ////////////////////////////////////////
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glUniform1i(glGetUniformLocation(prog,"uIsMarker"), 1);
    glBindVertexArray(markerVao);

    float hudDist = 0.f, hudRange = 1.f;
    const char* hudPreset = "demo";

    float nearestDist = 1e9f;
    for(auto& l : worldLights) 
    {
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

    glUniform1i(
    glGetUniformLocation(prog, "uIsMarker"),
    1);
    glBindVertexArray(objectMarkerVao);

    for (const auto& obj : worldVobs)
    {
        glm::mat4 markerModel =
            glm::translate(
                glm::mat4(1.f),
                obj.pos
            );

        glUniformMatrix4fv(
            glGetUniformLocation(prog, "uModel"),
            1,
            GL_FALSE,
            glm::value_ptr(markerModel)
        );

        // różne kolory dla różnych vobów
        glm::vec3 color = vobTypeColor(obj.type);

        glUniform3fv(
            glGetUniformLocation(prog, "uLightColor"),
            1,
            glm::value_ptr(color)
        );

        glDrawArrays(
            GL_TRIANGLES,
            0,
            GLsizei(objectMarkerVerts.size())
        );
    }

    //Wyswietlanie napisu  z typem VOBa
    glm::mat4 textOrtho =
    glm::ortho(
        0.f,
        float(fbw),
        float(fbh),
        0.f,
        -1.f,
        1.f
    );

    for(const auto& obj : worldVobs)
    {
        if(!isVobInView(
            obj.pos,
            g_cam,
            200.f,
            0.90f))
        {
            continue;
        }

        std::string label =
            std::string(vobTypeName(obj.type));

        if(!obj.visualName.empty())
        {
            label += "\n";
            label += obj.visualName;
        }

        drawWorldLabel(
            text,
            label,
            obj.pos + glm::vec3(0.f, 10.f, 0.f),
            view,
            proj,
            fbw,
            fbh,
            textOrtho,
            255, 255, 255, 255
        );
    }

    //////////////////////////////////////////////
    ///Wyswietlanie napisów HUD
    ////////////////////////////////////////////


    drawHud(text, fbw, fbh, g_cam, hudPreset, hudRange, hudDist, g_formulaMode,
       g_lightcorrection, g_tonemap, g_lightIntensity, g_fogEnabled, g_fogDensity,
       worldLights,
       view, proj, g_fpsSmoothed, g_texturesEnabled
      );

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

  for(auto& fog : fogPerLight)
    deleteFogBuffer(fog);

  glfwTerminate();
  return 0;
  }
