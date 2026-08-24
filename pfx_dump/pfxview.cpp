/*
   pfxview - podglad na zywo efektow czasteczkowych z Gothic/Gothic II z obsługa PARTICLEFX.DAT oraz VISUALFX.DAT.
*/

#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <zenkit/DaedalusScript.hh>
#include <zenkit/DaedalusVm.hh>
#include <zenkit/addon/daedalus.hh>
#include <zenkit/Stream.hh>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <memory>
#include <random>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

#include "texture_loader.h"

namespace fs = std::filesystem;

/* ---------------------------------------------------------------------
   Pomocnicze funkcje tekstowe
   --------------------------------------------------------------------- */

static bool containsIgnoreCase(const std::string& str, const std::string& subStr)
{
  if (subStr.empty()) return true;
  auto it = std::search(
    str.begin(), str.end(),
    subStr.begin(), subStr.end(),
    [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
  );
  return it != str.end();
}

static glm::vec3 parseVec3(const std::string& s, glm::vec3 fallback = glm::vec3(0.f))
{
  float v[3] = {fallback.x, fallback.y, fallback.z};
  const char* str = s.c_str();
  for(int i = 0; i < 3; ++i)
  {
    char* next = nullptr;
    float f = std::strtof(str, &next);
    if(str==next) break;
    v[i] = f;
    str = next;
  }
  return glm::vec3(v[0], v[1], v[2]);
}

static glm::vec2 parseVec2(const std::string& s, glm::vec2 fallback = glm::vec2(0.f))
{
  float v[2] = {fallback.x, fallback.y};
  const char* str = s.c_str();
  for(int i = 0; i < 2; ++i)
  {
    char* next = nullptr;
    float f = std::strtof(str, &next);
    if(str==next) break;
    v[i] = f;
    str = next;
  }
  return glm::vec2(v[0], v[1]);
}

/* ---------------------------------------------------------------------
   Parametry i metadane efektu
   --------------------------------------------------------------------- */
struct PfxParams
{
  std::string originDatFile;
  std::string loadedTexturePath;

  float       ppsValue = 0.f;
  std::string visName;

  std::string shpType = "POINT";
  glm::vec3   shpOffset = glm::vec3(0.f);
  glm::vec3   shpDim    = glm::vec3(0.f);
  bool        shpIsVolume = true;

  std::string dirMode = "RAND";
  float       dirAngleHead    = 0.f;
  float       dirAngleHeadVar = 0.f;
  float       dirAngleElev    = 90.f;
  float       dirAngleElevVar = 0.f;

  float       velAvg = 0.f;
  float       velVar = 0.f;
  float       lspAvg = 500.f;
  float       lspVar = 0.f;

  glm::vec3   gravity = glm::vec3(0.f);

  glm::vec3   colorStart = glm::vec3(1.f);
  glm::vec3   colorEnd   = glm::vec3(1.f);
  glm::vec2   sizeStart  = glm::vec2(10.f, 10.f);
  float       sizeEndScale = 1.f;
  float       alphaStart = 1.f;
  float       alphaEnd   = 1.f;
};

static PfxParams extractParams(const zenkit::IParticleEffect& p)
{
  PfxParams out;
  out.ppsValue      = p.pps_value;
  out.visName       = p.vis_name_s;

  out.shpType       = p.shp_type_s;
  out.shpOffset     = parseVec3(p.shp_offset_vec_s);
  out.shpDim        = parseVec3(p.shp_dim_s);
  out.shpIsVolume   = p.shp_is_volume!=0;

  out.dirMode       = p.dir_mode_s;
  out.dirAngleHead    = p.dir_angle_head;
  out.dirAngleHeadVar = p.dir_angle_head_var;
  out.dirAngleElev    = p.dir_angle_elev;
  out.dirAngleElevVar = p.dir_angle_elev_var;

  out.velAvg        = p.vel_avg;
  out.velVar        = p.vel_var;
  out.lspAvg        = p.lsp_part_avg>0.f ? p.lsp_part_avg : 500.f;
  out.lspVar        = p.lsp_part_var;

  out.gravity       = parseVec3(p.fly_gravity_s);

  out.colorStart    = parseVec3(p.vis_tex_color_start_s, glm::vec3(255.f))/255.f;
  out.colorEnd      = parseVec3(p.vis_tex_color_end_s,   glm::vec3(255.f))/255.f;

  out.sizeStart     = parseVec2(p.vis_size_start_s, glm::vec2(10.f));
  out.sizeEndScale  = p.vis_size_end_scale>0.f ? p.vis_size_end_scale : 1.f;

  out.alphaStart    = std::clamp(p.vis_alpha_start/255.f, 0.f, 1.f);
  out.alphaEnd      = std::clamp(p.vis_alpha_end/255.f,   0.f, 1.f);

  return out;
}

/* ---------------------------------------------------------------------
   ParticleLibrary - wielo-plikowy menedżer skryptów .DAT
   --------------------------------------------------------------------- */
struct LoadedScript
{
  std::string fileName;
  zenkit::DaedalusScript script;
  std::unique_ptr<zenkit::DaedalusVm> vm;
};

class ParticleLibrary
{
public:
  bool loadFile(const std::string& datPath)
  {
    std::string filename = fs::path(datPath).filename().string();

    auto loaded = std::make_unique<LoadedScript>();
    loaded->fileName = filename;

    try
    {
      auto reader = zenkit::Read::from(datPath);
      loaded->script.load(reader.get());
    }
    catch(const std::exception& e)
    {
      fprintf(stderr, "[Library] Nie udalo sie wczytac %s: %s\n", datPath.c_str(), e.what());
      return false;
    }

    try
    {
      zenkit::IParticleEffect::register_(loaded->script);
    }
    catch(...) {}

    std::vector<uint32_t> parentIndices;
    const char* classNames[] = { "C_PARTICLEFX", "C_PARTICLEFXEMITHP", "C_XIVISUALFX", "CFX" };
    
    for(const char* className : classNames)
    {
      auto* cls = loaded->script.find_symbol_by_name(className);
      if(cls != nullptr) parentIndices.push_back(cls->index());
    }

    for(auto& sym : loaded->script.symbols())
    {
      if(sym.type() == zenkit::DaedalusDataType::INSTANCE || sym.type() == zenkit::DaedalusDataType::PROTOTYPE)
      {
        bool isPfx = false;
        for(uint32_t pIdx : parentIndices)
        {
          if(sym.parent() == pIdx) { isPfx = true; break; }
        }

        if(isPfx || containsIgnoreCase(sym.name(), "PFX") || containsIgnoreCase(sym.name(), "POTION") || containsIgnoreCase(sym.name(), "SPELL"))
        {
          if(effectOriginMap.find(sym.name()) == effectOriginMap.end())
          {
            effectNames.push_back(sym.name());
            effectOriginMap[sym.name()] = filename;
          }
        }
      }
    }

    loaded->vm = std::make_unique<zenkit::DaedalusVm>(std::move(loaded->script), zenkit::DaedalusVmExecutionFlag::ALLOW_NULL_INSTANCE_ACCESS);
    scripts.push_back(std::move(loaded));

    std::sort(effectNames.begin(), effectNames.end());
    return true;
  }

  const std::vector<std::string>& names() const { return effectNames; }

  std::string getOriginFile(const std::string& name) const
  {
    auto it = effectOriginMap.find(name);
    return it != effectOriginMap.end() ? it->second : "Nieznany";
  }

  bool getParams(const std::string& name, PfxParams& out) const
  {
    for(const auto& sc : scripts)
    {
      auto* sym = sc->vm->find_symbol_by_name(name);
      if(!sym) continue;

      auto pfx = std::make_shared<zenkit::IParticleEffect>();
      pfx->vis_tex_is_quadpoly = 1;

      try
      {
        sc->vm->init_instance(pfx, sym);
        out = extractParams(*pfx);
        out.originDatFile = sc->fileName;
        return true;
      }
      catch(...)
      {
        return false;
      }
    }
    return false;
  }

private:
  std::vector<std::unique_ptr<LoadedScript>> scripts;
  std::vector<std::string>                   effectNames;
  std::unordered_map<std::string, std::string> effectOriginMap;
};

/* ---------------------------------------------------------------------
   Rozwiązanie ścieżek
   --------------------------------------------------------------------- */
static std::vector<std::string> discoverDatFiles(int argc, char** argv)
{
  std::vector<std::string> result;
  std::string baseDir;

  if(argc >= 2)
  {
    baseDir = fs::path(argv[1]).parent_path().string();
    result.push_back(argv[1]);
  }
  else
  {
    const char* env = std::getenv("GOTHIC2_DIR");
    if(env && env[0] != '\0')
      baseDir = std::string(env) + "/_Work/Data/Scripts/_compiled/";
  }

  if(!baseDir.empty() && fs::exists(baseDir))
  {
    std::string pfx = baseDir + "/PARTICLEFX.DAT";
    std::string vfx = baseDir + "/VISUALFX.DAT";

    if(fs::exists(pfx) && std::find(result.begin(), result.end(), pfx) == result.end())
      result.push_back(pfx);
    if(fs::exists(vfx) && std::find(result.begin(), result.end(), vfx) == result.end())
      result.push_back(vfx);
  }

  return result;
}

/* ---------------------------------------------------------------------
   Fizyka i Symulacja Cząsteczek
   --------------------------------------------------------------------- */
struct LiveParticle
{
  glm::vec3 pos;
  glm::vec3 vel;
  float     age      = 0.f;
  float     lifetime = 500.f;
};

struct ParticleVertex
{
  glm::vec3 pos;
  float     size;
  glm::vec4 colorAlpha;
};

static std::mt19937 g_rng(std::random_device{}());

static float randRange(float lo, float hi)
{
  if(hi<lo) std::swap(lo,hi);
  std::uniform_real_distribution<float> d(lo,hi);
  return d(g_rng);
}

static glm::vec3 sampleEmitterPos(const PfxParams& p)
{
  glm::vec3 local(0.f);

  if(p.shpType=="LINE") local.x = randRange(-p.shpDim.x*0.5f, p.shpDim.x*0.5f);
  else if(p.shpType=="BOX")
  {
    local.x = randRange(-p.shpDim.x*0.5f, p.shpDim.x*0.5f);
    local.y = randRange(-p.shpDim.y*0.5f, p.shpDim.y*0.5f);
    local.z = randRange(-p.shpDim.z*0.5f, p.shpDim.z*0.5f);
  }
  else if(p.shpType=="CIRCLE")
  {
    float r = p.shpDim.x;
    float ang = randRange(0.f, 6.2831853f);
    float rr = p.shpIsVolume ? r*std::sqrt(randRange(0.f,1.f)) : r;
    local.x = rr*std::cos(ang);
    local.z = rr*std::sin(ang);
  }
  else if(p.shpType=="SPHERE")
  {
    float r = p.shpDim.x;
    float az = randRange(0.f, 6.2831853f);
    float el = randRange(-1.f,1.f);
    float rr = p.shpIsVolume ? r*std::cbrt(randRange(0.f,1.f)) : r;
    float sinEl = el;
    float cosEl = std::sqrt(std::max(0.f,1.f-sinEl*sinEl));
    local = glm::vec3(rr*cosEl*std::cos(az), rr*sinEl, rr*cosEl*std::sin(az));
  }

  return local + p.shpOffset;
}

static glm::vec3 baseDirection(const PfxParams& p)
{
  float headRad = glm::radians(p.dirAngleHead);
  float elevRad = glm::radians(p.dirAngleElev);
  float cosElev = std::cos(elevRad);
  return glm::vec3(cosElev*std::sin(headRad), std::sin(elevRad), cosElev*std::cos(headRad));
}

static glm::vec3 sampleDirection(const PfxParams& p)
{
  if(p.dirMode=="RAND")
  {
    float head = randRange(p.dirAngleHead-p.dirAngleHeadVar, p.dirAngleHead+p.dirAngleHeadVar);
    float elev = randRange(p.dirAngleElev-p.dirAngleElevVar, p.dirAngleElev+p.dirAngleElevVar);
    float headRad = glm::radians(head);
    float elevRad = glm::radians(elev);
    float cosElev = std::cos(elevRad);
    return glm::vec3(cosElev*std::sin(headRad), std::sin(elevRad), cosElev*std::cos(headRad));
  }
  return baseDirection(p);
}

static float estimateEffectRadius(const PfxParams& p)
{
  float shapeExtent = std::max({p.shpDim.x, p.shpDim.y, p.shpDim.z}) * 0.5f;
  float speedMax     = p.velAvg + p.velVar;
  float lifeSec       = (p.lspAvg + p.lspVar) / 1000.f;
  float travel        = speedMax * lifeSec;
  float gravityDrop   = 0.5f * glm::length(p.gravity) * lifeSec * lifeSec;

  return std::max(shapeExtent + travel + gravityDrop, 50.f);
}

static void spawnParticle(const PfxParams& p, std::vector<LiveParticle>& particles)
{
  LiveParticle np;
  np.pos = sampleEmitterPos(p);
  glm::vec3 dir = sampleDirection(p);
  float speed = std::max(0.f, randRange(p.velAvg-p.velVar, p.velAvg+p.velVar));
  np.vel = dir*speed;
  np.age = 0.f;
  np.lifetime = std::max(10.f, randRange(p.lspAvg-p.lspVar, p.lspAvg+p.lspVar));
  particles.push_back(np);
}

/* ---------------------------------------------------------------------
   Shadery
   --------------------------------------------------------------------- */
static const char* VERT_SRC = R"GLSL(
#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aSize;
layout(location = 2) in vec4  aColorAlpha;

uniform mat4 uView;
uniform mat4 uProj;
uniform float uViewportHeight;

out vec4 vColorAlpha;

void main() {
  vec4 viewPos = uView * vec4(aPos, 1.0);
  gl_Position = uProj * viewPos;

  if (viewPos.z < 0.0) {
    gl_PointSize = uProj[1][1] * (aSize / -viewPos.z) * (uViewportHeight * 0.5);
  } else {
    gl_PointSize = 0.0;
  }

  vColorAlpha = aColorAlpha;
}
)GLSL";

static const char* FRAG_SRC = R"GLSL(
#version 330 core
in vec4 vColorAlpha;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform bool uUseTexture;

void main() {
  vec4 texCol = vec4(1.0);
  
  if (uUseTexture) {
    texCol = texture(uTexture, gl_PointCoord);
  } else {
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(c, c);
    if (r2 > 1.0) discard;
    texCol = vec4(vec3(1.0), 1.0 - r2);
  }

  FragColor = texCol * vColorAlpha;
}
)GLSL";

static const char* FLOOR_VERT_SRC = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uView;
uniform mat4 uProj;
void main() { gl_Position = uProj * uView * vec4(aPos,1.0); }
)GLSL";

static const char* FLOOR_FRAG_SRC = R"GLSL(
#version 330 core
out vec4 FragColor;
void main() { FragColor = vec4(0.35,0.35,0.4,0.18); }
)GLSL";

static GLuint compileShader(GLenum type, const char* src)
{
  GLuint sh = glCreateShader(type);
  glShaderSource(sh, 1, &src, nullptr);
  glCompileShader(sh);
  GLint ok = 0;
  glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
  if(!ok)
  {
    char log[4096];
    glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
    fprintf(stderr, "Blad kompilacji shadera:\n%s\n", log);
  }
  return sh;
}

static GLuint linkProgram(GLuint vs, GLuint fs)
{
  GLuint prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  GLint ok = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if(!ok)
  {
    char log[4096];
    glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
    fprintf(stderr, "Blad linkowania programu:\n%s\n", log);
  }
  return prog;
}

/* ---------------------------------------------------------------------
   Kamera Orbitująca
   --------------------------------------------------------------------- */
struct Camera
{
  glm::vec3 targetPos = {0.f, 0.f, 0.f};
  float     distance  = 300.f;
  float     yaw       = -90.f;
  float     pitch     = 15.f;
  float     speed     = 300.f;

  glm::vec3 getPosition() const
  {
    float radYaw   = glm::radians(yaw);
    float radPitch = glm::radians(pitch);

    glm::vec3 dir;
    dir.x = std::cos(radYaw) * std::cos(radPitch);
    dir.y = std::sin(radPitch);
    dir.z = std::sin(radYaw) * std::cos(radPitch);

    return targetPos - dir * distance;
  }

  glm::vec3 front() const { return glm::normalize(targetPos - getPosition()); }
  glm::vec3 right() const { return glm::normalize(glm::cross(front(), glm::vec3(0, 1, 0))); }
  glm::mat4 view() const  { return glm::lookAt(getPosition(), targetPos, glm::vec3(0, 1, 0)); }
};

static Camera g_cam;
static bool   g_keys[512] = {};
static double g_lastX = 400, g_lastY = 300;
static bool   g_firstMouse = true;
static bool   g_lookActive = false;
static bool   g_resetRequested = false;
static const float FIXED_FOV = 60.f;

static void scrollCallback(GLFWwindow*, double, double yoffset)
{
  if(ImGui::GetIO().WantCaptureMouse) return;
  float zoomFactor = 1.15f;
  if(yoffset > 0) g_cam.distance /= zoomFactor;
  else if(yoffset < 0) g_cam.distance *= zoomFactor;
  g_cam.distance = std::clamp(g_cam.distance, 5.f, 50000.f);
}

static void reframeCameraToEffect(const PfxParams& p)
{
  float radius = estimateEffectRadius(p);
  g_cam.targetPos = p.shpOffset + baseDirection(p) * (radius * 0.2f);
  g_cam.distance = std::clamp(radius * 2.5f, 20.f, 30000.f);
}

static void keyCallback(GLFWwindow* w, int key, int, int action, int)
{
  if(action==GLFW_PRESS || action==GLFW_RELEASE) g_keys[key] = (action==GLFW_PRESS);
  if(action!=GLFW_PRESS) return;
  if(key==GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
  if(key==GLFW_KEY_R) g_resetRequested = true;
}

static void mouseButtonCallback(GLFWwindow* w, int button, int action, int)
{
  if(button!=GLFW_MOUSE_BUTTON_LEFT) return;
  if(action==GLFW_PRESS && !ImGui::GetIO().WantCaptureMouse)
  {
    g_lookActive = true;
    g_firstMouse = true;
    glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  }
  else if(action==GLFW_RELEASE)
  {
    g_lookActive = false;
    glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  }
}

static void cursorCallback(GLFWwindow*, double x, double y)
{
  if(!g_lookActive) { g_lastX=x; g_lastY=y; return; }
  if(g_firstMouse) { g_lastX=x; g_lastY=y; g_firstMouse=false; }
  double dx = x-g_lastX, dy = g_lastY-y;
  g_lastX=x; g_lastY=y;
  const float sens = 0.15f;
  g_cam.yaw   += float(dx)*sens;
  g_cam.pitch += float(dy)*sens;
  g_cam.pitch  = std::clamp(g_cam.pitch, -89.f, 89.f);
}

static void loadSelectedEffect(const std::string& name, const ParticleLibrary& lib, PfxParams& currentParams, 
                                std::vector<LiveParticle>& particles, float& spawnAccum, bool autoZoom, 
                                Texture2D& currentTexture, const std::vector<std::string>& datPaths)
{
  if(lib.getParams(name, currentParams))
  {
    particles.clear();
    spawnAccum = 0.f;
    if(autoZoom) reframeCameraToEffect(currentParams);

    std::string gothicDir;
    const char* envDir = std::getenv("GOTHIC2_DIR");

    if (envDir && envDir[0] != '\0')
    {
      gothicDir = envDir;
    }
    else if(!datPaths.empty())
    {
      fs::path p(datPaths[0]);
      while (p.has_parent_path() && p.filename() != "_Work") p = p.parent_path();
      gothicDir = (p.filename() == "_Work") ? p.parent_path().string() : fs::path(datPaths[0]).parent_path().string();
    }

    currentTexture.free();
    currentParams.loadedTexturePath.clear();

    if (!currentParams.visName.empty())
    {
      std::string fullPath = TextureLoader::resolveGothicTexturePath(currentParams.visName, gothicDir);

      if (!fullPath.empty())
      {
        currentTexture = TextureLoader::loadFromFile(fullPath, false);
        if (currentTexture.valid) currentParams.loadedTexturePath = fullPath;
      }
    }
  }
}

int main(int argc, char** argv)
{
  auto datPaths = discoverDatFiles(argc, argv);
  if(datPaths.empty())
  {
    fprintf(stderr, "Brak sciezki do PARTICLEFX.DAT lub VISUALFX.DAT.\n");
    return 1;
  }

  ParticleLibrary lib;
  for(const auto& path : datPaths)
  {
    if(lib.loadFile(path))
      printf("Zaladowano skrypt: %s\n", path.c_str());
  }

  printf("Suma wczytanych efektow: %zu\n", lib.names().size());

  if(!glfwInit()) return 1;
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow* win = glfwCreateWindow(1400, 800, "pfxview - Gothic Particle Inspector", nullptr, nullptr);
  if(!win) { glfwTerminate(); return 1; }
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);
  glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  glfwSetKeyCallback(win, keyCallback);
  glfwSetCursorPosCallback(win, cursorCallback);
  glfwSetMouseButtonCallback(win, mouseButtonCallback);
  glfwSetScrollCallback(win, scrollCallback);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_PROGRAM_POINT_SIZE);

  GLuint pvs = compileShader(GL_VERTEX_SHADER, VERT_SRC);
  GLuint pfs = compileShader(GL_FRAGMENT_SHADER, FRAG_SRC);
  GLuint particleProg = linkProgram(pvs, pfs);
  glDeleteShader(pvs); glDeleteShader(pfs);

  GLuint fvs = compileShader(GL_VERTEX_SHADER, FLOOR_VERT_SRC);
  GLuint ffs = compileShader(GL_FRAGMENT_SHADER, FLOOR_FRAG_SRC);
  GLuint floorProg = linkProgram(fvs, ffs);
  glDeleteShader(fvs); glDeleteShader(ffs);

  float R = 2000.f;
  float floorVerts[] = { -R,0,-R, R,0,-R, R,0,R, -R,0,-R, R,0,R, -R,0,R };
  GLuint floorVao, floorVbo;
  glGenVertexArrays(1,&floorVao);
  glGenBuffers(1,&floorVbo);
  glBindVertexArray(floorVao);
  glBindBuffer(GL_ARRAY_BUFFER, floorVbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(floorVerts), floorVerts, GL_STATIC_DRAW);
  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
  glEnableVertexAttribArray(0);

  const size_t MAX_PARTICLES = 50000;
  GLuint pVao, pVbo;
  glGenVertexArrays(1,&pVao);
  glGenBuffers(1,&pVbo);
  glBindVertexArray(pVao);
  glBindBuffer(GL_ARRAY_BUFFER, pVbo);
  glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES*sizeof(ParticleVertex), nullptr, GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(ParticleVertex),(void*)offsetof(ParticleVertex,pos));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1,1,GL_FLOAT,GL_FALSE,sizeof(ParticleVertex),(void*)offsetof(ParticleVertex,size));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(ParticleVertex),(void*)offsetof(ParticleVertex,colorAlpha));
  glEnableVertexAttribArray(2);

IMGUI_CHECKVERSION();
ImGui::CreateContext();
ImGuiIO& io = ImGui::GetIO();

// Zakres glifów dla polskich znaków (Latin-1 + Latin Extended-A)
static const ImWchar polish_ranges[] = {
    0x0020, 0x00FF, // Basic Latin + Latin Supplement
    0x0100, 0x017F, // Latin Extended-A (ą, ć, ę, ł, ń, ó, ś, ź, ż)
    0,
};

const char* fontPaths[] = {
    // Arch Linux (standardowe lokalizacje)
    "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/gnu-free/FreeSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    
    // Debian / Ubuntu
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    
    // Windows
    "C:\\Windows\\Fonts\\segoeui.ttf",
    "C:\\Windows\\Fonts\\arial.ttf"
};

std::string loadedFontPath = "";
ImFont* font = nullptr;

for (const char* fontPath : fontPaths)
{
    if (std::filesystem::exists(fontPath))
    {
        font = io.Fonts->AddFontFromFileTTF(fontPath, 16.0f, nullptr, polish_ranges);
        if (font) {
            loadedFontPath = fontPath;
            printf("[ImGui LOG] Pomyślnie załadowano czcionkę z polskimi znakami: %s\n", fontPath);
            break;
        }
    }
}

if (!font)
{
    io.Fonts->AddFontDefault();
    printf("[ImGui LOG] OSTRZEŻENIE: Nie odnaleziono żadnej czcionki TTF! Użyto domyślnej ProggyClean (brak obsługi polskich znaków).\n");
}

ImGui::StyleColorsDark();
ImGui_ImplGlfw_InitForOpenGL(win, true);
ImGui_ImplOpenGL3_Init("#version 330");

  std::vector<LiveParticle>   particles;
  std::vector<ParticleVertex> renderBuf;
  particles.reserve(MAX_PARTICLES);
  renderBuf.reserve(MAX_PARTICLES);

  PfxParams   currentParams;
  Texture2D   currentTexture;
  std::string selectedName;
  char        searchBuffer[128] = "";
  float       spawnAccum = 0.f;
  bool        autoZoom = true;
  double      lastTime = glfwGetTime();

  while(!glfwWindowShouldClose(win))
  {
    double now = glfwGetTime();
    float dt = std::min(float(now-lastTime), 0.05f);
    lastTime = now;

    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    std::string searchFilter(searchBuffer);
    std::vector<std::string> filteredNames;
    for (const auto& name : lib.names())
    {
      if (containsIgnoreCase(name, searchFilter)) filteredNames.push_back(name);
    }

    bool selectionChangedByKeys = false;
    if (!ImGui::GetIO().WantTextInput && !filteredNames.empty())
    {
      int currentIndex = -1;
      for (size_t i = 0; i < filteredNames.size(); ++i)
      {
        if (filteredNames[i] == selectedName) { currentIndex = static_cast<int>(i); break; }
      }

      int newIndex = currentIndex;
      if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false))
        newIndex = (currentIndex <= 0) ? static_cast<int>(filteredNames.size()) - 1 : currentIndex - 1;
      else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false))
        newIndex = (currentIndex < 0 || currentIndex >= static_cast<int>(filteredNames.size()) - 1) ? 0 : currentIndex + 1;

      if (newIndex != currentIndex && newIndex >= 0 && newIndex < static_cast<int>(filteredNames.size()))
      {
        selectedName = filteredNames[newIndex];
        loadSelectedEffect(selectedName, lib, currentParams, particles, spawnAccum, autoZoom, currentTexture, datPaths);
        selectionChangedByKeys = true;
      }
    }

    ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340,800), ImGuiCond_FirstUseEver);
    ImGui::Begin("Efekty (DAT)");
    ImGui::Text("Efekty na liscie: %zu", lib.names().size());
    ImGui::Checkbox("Auto Zoom", &autoZoom);
    ImGui::SameLine();
    if(!selectedName.empty() && ImGui::Button("Dopasuj")) reframeCameraToEffect(currentParams);

    ImGui::InputText("##szukaj", searchBuffer, IM_ARRAYSIZE(searchBuffer));
    ImGui::SameLine();
    if(ImGui::Button("X")) searchBuffer[0] = '\0';

    ImGui::Separator();
    ImGui::BeginChild("lista_efektow");
    
    for(const auto& n : filteredNames)
    {
      bool isSelected = (n == selectedName);
      std::string label = n + " [" + lib.getOriginFile(n) + "]";
      
      if(ImGui::Selectable(label.c_str(), isSelected))
      {
        if(n != selectedName)
        {
          selectedName = n;
          loadSelectedEffect(selectedName, lib, currentParams, particles, spawnAccum, autoZoom, currentTexture, datPaths);
        }
      }

      if (isSelected && selectionChangedByKeys) ImGui::SetScrollHereY(0.5f);
    }
    ImGui::EndChild();
    ImGui::End();

    /* Szczegółowy Panel Inspekcyjny */
    ImGui::SetNextWindowPos(ImVec2(350, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 480), ImGuiCond_FirstUseEver);
    ImGui::Begin("Inspector Szczegolow Efektu");

    if (!selectedName.empty())
    {
      ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Nazwa Instancji:");
      ImGui::SameLine(); ImGui::Text("%s", selectedName.c_str());

      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Plik Skryptu (.DAT):");
      ImGui::SameLine(); ImGui::Text("%s", currentParams.originDatFile.c_str());

      ImGui::Separator();
      ImGui::TextUnformatted("--- TEKSTURA ---");
      ImGui::Text("Deklarowana w Daedalus: %s", currentParams.visName.empty() ? "(Brak)" : currentParams.visName.c_str());
      
      if (!currentParams.loadedTexturePath.empty())
      {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Sciezka pliku:");
        ImGui::TextWrapped("%s", currentParams.loadedTexturePath.c_str());
      }
      else
      {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Stan: Nieodnaleziona w katalogach/_compiled/VDF");
      }

      if (currentTexture.valid && currentTexture.id != 0)
      {
        ImGui::Text("Rozmiar GL: %dx%d px", currentTexture.width, currentTexture.height);
        ImGui::Image((void*)(intptr_t)currentTexture.id, ImVec2(80, 80));
      }

      ImGui::Separator();
      ImGui::TextUnformatted("--- PARAMETRY EMISJI ---");
      ImGui::Text("Szybkosć Emisji (ppsValue): %.2f", currentParams.ppsValue);
      ImGui::Text("Ksztalt Emitera (shpType): %s", currentParams.shpType.c_str());
      ImGui::Text("Wymiary Emitera: [%.1f, %.1f, %.1f]", currentParams.shpDim.x, currentParams.shpDim.y, currentParams.shpDim.z);
      ImGui::Text("Predkosc Cząsteczek: %.1f (+-%.1f)", currentParams.velAvg, currentParams.velVar);
      ImGui::Text("Czas Zycia: %.0f ms (+-%.0f ms)", currentParams.lspAvg, currentParams.lspVar);
      ImGui::Text("Grawitacja: [%.1f, %.1f, %.1f]", currentParams.gravity.x, currentParams.gravity.y, currentParams.gravity.z);
      ImGui::Text("Skala Rozmiaru (Start->End): [%.1f,%.1f] -> x%.2f", currentParams.sizeStart.x, currentParams.sizeStart.y, currentParams.sizeEndScale);
    }
    else
    {
      ImGui::TextDisabled("Wybierz efekt z listy, aby wyswietlic szczegoly.");
    }
    ImGui::End();

    /* Poruszanie kamerą */
    if(!ImGui::GetIO().WantCaptureKeyboard)
    {
      float speed = g_cam.speed * dt * (g_keys[GLFW_KEY_LEFT_SHIFT] ? 3.f : 1.f);
      if(g_keys[GLFW_KEY_W]) g_cam.targetPos += g_cam.front()*speed;
      if(g_keys[GLFW_KEY_S]) g_cam.targetPos -= g_cam.front()*speed;
      if(g_keys[GLFW_KEY_A]) g_cam.targetPos -= g_cam.right()*speed;
      if(g_keys[GLFW_KEY_D]) g_cam.targetPos += g_cam.right()*speed;
      if(g_keys[GLFW_KEY_SPACE])        g_cam.targetPos.y += speed;
      if(g_keys[GLFW_KEY_LEFT_CONTROL]) g_cam.targetPos.y -= speed;
    }

    if(g_resetRequested)
    {
      particles.clear();
      spawnAccum = 0.f;
      g_resetRequested = false;
    }

    if (!selectedName.empty())
    {
      if (currentParams.ppsValue > 0.f)
      {
        spawnAccum += currentParams.ppsValue * dt;
        while (spawnAccum >= 1.f && particles.size() < MAX_PARTICLES)
        {
          spawnParticle(currentParams, particles);
          spawnAccum -= 1.f;
        }
      }
      else if (particles.empty())
      {
        for (int i = 0; i < 30 && particles.size() < MAX_PARTICLES; ++i) spawnParticle(currentParams, particles);
      }
    }

    for(size_t i=0;i<particles.size();)
    {
      LiveParticle& lp = particles[i];
      lp.age += dt*1000.f;
      if(lp.age>=lp.lifetime)
      {
        lp = particles.back();
        particles.pop_back();
        continue;
      }
      lp.vel += currentParams.gravity*dt;
      lp.pos += lp.vel*dt;
      ++i;
    }

    renderBuf.clear();
    for(auto& lp : particles)
    {
      float t = std::clamp(lp.age/lp.lifetime, 0.f, 1.f);
      float sizeCm = glm::mix((currentParams.sizeStart.x+currentParams.sizeStart.y)*0.5f,
                               (currentParams.sizeStart.x+currentParams.sizeStart.y)*0.5f*currentParams.sizeEndScale, t);
      glm::vec3 col = glm::mix(currentParams.colorStart, currentParams.colorEnd, t);
      float alpha = glm::mix(currentParams.alphaStart, currentParams.alphaEnd, t);

      ParticleVertex v;
      v.pos = lp.pos;
      v.size = sizeCm;
      v.colorAlpha = glm::vec4(col, alpha);
      renderBuf.push_back(v);
    }

    if(!renderBuf.empty())
    {
      glBindBuffer(GL_ARRAY_BUFFER, pVbo);
      glBufferSubData(GL_ARRAY_BUFFER, 0, GLsizeiptr(renderBuf.size()*sizeof(ParticleVertex)), renderBuf.data());
    }

    int fbw, fbh;
    glfwGetFramebufferSize(win, &fbw, &fbh);
    glViewport(0,0,fbw,fbh);
    glClearColor(0.02f,0.02f,0.03f,1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 proj = glm::perspective(glm::radians(FIXED_FOV), float(fbw)/float(fbh), 1.f, 100000.f);
    glm::mat4 view = g_cam.view();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glUseProgram(floorProg);
    glUniformMatrix4fv(glGetUniformLocation(floorProg,"uView"),1,GL_FALSE,glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(floorProg,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
    glBindVertexArray(floorVao);
    glDrawArrays(GL_TRIANGLES,0,6);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glUseProgram(particleProg);
    glUniformMatrix4fv(glGetUniformLocation(particleProg,"uView"),1,GL_FALSE,glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(particleProg,"uProj"),1,GL_FALSE,glm::value_ptr(proj));
    glUniform1f(glGetUniformLocation(particleProg, "uViewportHeight"), float(fbh));

    GLint useTexLoc = glGetUniformLocation(particleProg, "uUseTexture");
    GLint texLoc    = glGetUniformLocation(particleProg, "uTexture");

    if (currentTexture.valid && currentTexture.id != 0)
    {
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, currentTexture.id);
      glUniform1i(texLoc, 0);
      glUniform1i(useTexLoc, 1);
    }
    else
    {
      glUniform1i(useTexLoc, 0);
    }

    glBindVertexArray(pVao);
    glDrawArrays(GL_POINTS, 0, GLsizei(renderBuf.size()));
    glDepthMask(GL_TRUE);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(win);
  }

  currentTexture.free();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwTerminate();
  return 0;
}