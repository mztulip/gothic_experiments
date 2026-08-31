// gothic_bottle_demo
//
// Prosty przyklad OpenGL (core profile 3.3) wykorzystujacy libepoxy zamiast GLAD
// do ladowania funkcji OpenGL. Epoxy sam wybiera odpowiednie wskazniki funkcji
// przy kazdym wywolaniu, wiec nie trzeba recznie inicjalizowac loadera
// (jak np. gladLoadGL) - wystarczy miec aktywny kontekst OpenGL (z GLFW).
//
// Scena zawiera:
//   - podloge z proceduralnie wygenerowana tekstura "ziemi" (szum + plamy)
//   - butelke (geometria generowana metoda "lathe/revolve") w jednolitym kolorze
//   - efekt magicznej aury (pfx) w stylu Gothic 1/2: male iskry/gwiazdki
//     krazace wokol butelki, pojawiajace sie i znikajace (additive blending)
//
// Sterowanie:
//   W/A/S/D - ruch kamery
//   Mysz (przytrzymaj prawy przycisk) - rozgladanie sie
//   Scroll - zoom (FOV)
//   ESC - wyjscie

#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <random>
#include <chrono>

// ---------------------------------------------------------------------------
// Parametry okna / globalne
// ---------------------------------------------------------------------------
static int   g_windowWidth  = 1280;
static int   g_windowHeight = 720;

// ---------------------------------------------------------------------------
// Kamera swobodna (fly camera)
// ---------------------------------------------------------------------------
struct Camera {
    glm::vec3 position = glm::vec3(0.0f, 1.6f, 4.0f);
    float yaw   = -90.0f; // patrzymy w -Z na starcie
    float pitch = -10.0f;
    float fov   = 60.0f;

    glm::vec3 front() const {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        return glm::normalize(f);
    }
    glm::vec3 right() const {
        return glm::normalize(glm::cross(front(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }
    glm::mat4 view() const {
        return glm::lookAt(position, position + front(), glm::vec3(0.0f, 1.0f, 0.0f));
    }
};

static Camera g_camera;
static bool   g_rotatingView = false;
static double g_lastMouseX = 0.0, g_lastMouseY = 0.0;
static bool   g_firstMouse = true;

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        g_rotatingView = (action == GLFW_PRESS);
        if (g_rotatingView) g_firstMouse = true;
    }
}

static void cursorPosCallback(GLFWwindow* /*window*/, double xpos, double ypos) {
    if (!g_rotatingView) { g_lastMouseX = xpos; g_lastMouseY = ypos; return; }
    if (g_firstMouse) { g_lastMouseX = xpos; g_lastMouseY = ypos; g_firstMouse = false; }

    float dx = float(xpos - g_lastMouseX);
    float dy = float(g_lastMouseY - ypos); // odwrocone Y
    g_lastMouseX = xpos;
    g_lastMouseY = ypos;

    const float sensitivity = 0.12f;
    g_camera.yaw   += dx * sensitivity;
    g_camera.pitch += dy * sensitivity;
    g_camera.pitch = glm::clamp(g_camera.pitch, -89.0f, 89.0f);
}

static void scrollCallback(GLFWwindow* /*window*/, double /*xoff*/, double yoff) {
    g_camera.fov -= float(yoff) * 2.0f;
    g_camera.fov = glm::clamp(g_camera.fov, 20.0f, 90.0f);
}

static void framebufferSizeCallback(GLFWwindow* /*window*/, int w, int h) {
    g_windowWidth = w; g_windowHeight = h;
    glViewport(0, 0, w, h);
}

static void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float speed = 3.0f * dt;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) speed *= 3.0f;

    glm::vec3 f = g_camera.front();
    glm::vec3 r = g_camera.right();

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) g_camera.position += f * speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) g_camera.position -= f * speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) g_camera.position -= r * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) g_camera.position += r * speed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) g_camera.position.y += speed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) g_camera.position.y -= speed;
}

// ---------------------------------------------------------------------------
// Pomocnicze: kompilacja shaderow
// ---------------------------------------------------------------------------
static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Blad kompilacji shadera: " << log << std::endl;
    }
    return shader;
}

static GLuint linkProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::cerr << "Blad linkowania programu: " << log << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ---------------------------------------------------------------------------
// Shadery: podloga (tekstura + prosta oswietlenie kierunkowe)
// ---------------------------------------------------------------------------
static const char* groundVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUV;

void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vNormal = mat3(uModel) * aNormal;
    vUV = aUV;
    gl_Position = uProj * uView * world;
}
)GLSL";

static const char* groundFS = R"GLSL(
#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uGroundTex;
uniform vec3 uLightDir;
uniform vec3 uViewPos;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);

    float ambient = 0.35;
    float diff = max(dot(N, L), 0.0);

    vec3 texColor = texture(uGroundTex, vUV).rgb;
    vec3 color = texColor * (ambient + diff * 0.8);

    FragColor = vec4(color, 1.0);
}
)GLSL";

// ---------------------------------------------------------------------------
// Shadery: butelka (solid color, blinn-phong)
// ---------------------------------------------------------------------------
static const char* bottleVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vNormal;
out vec3 vWorldPos;

void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vNormal = mat3(uModel) * aNormal;
    gl_Position = uProj * uView * world;
}
)GLSL";

static const char* bottleFS = R"GLSL(
#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
out vec4 FragColor;

uniform vec3 uBaseColor;
uniform vec3 uLightDir;
uniform vec3 uViewPos;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 H = normalize(L + V);

    float ambient = 0.25;
    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 48.0);

    vec3 color = uBaseColor * (ambient + diff * 0.75) + vec3(1.0) * spec * 0.5;
    FragColor = vec4(color, 1.0);
}
)GLSL";

// ---------------------------------------------------------------------------
// Shadery: czastki aury (point sprites, additive blending)
// ---------------------------------------------------------------------------
static const char* particleVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aAlpha;
layout(location = 2) in float aSize;
layout(location = 3) in vec3  aColor;

uniform mat4 uView;
uniform mat4 uProj;

out float vAlpha;
out vec3  vColor;

void main() {
    vAlpha = aAlpha;
    vColor = aColor;
    vec4 viewPos = uView * vec4(aPos, 1.0);
    gl_Position = uProj * viewPos;

    // rozmiar punktu maleje z odlegloscia od kamery (perspektywa)
    float dist = max(-viewPos.z, 0.001);
    gl_PointSize = aSize * (250.0 / dist);
}
)GLSL";

static const char* particleFS = R"GLSL(
#version 330 core
in float vAlpha;
in vec3  vColor;
out vec4 FragColor;

void main() {
    // gl_PointCoord: (0,0) - (1,1) w obrebie kwadratu point sprite'a
    vec2 c = gl_PointCoord - vec2(0.5);
    float d = length(c) * 2.0; // 0 w centrum, 1 na krawedzi

    if (d > 1.0) discard;

    // miekki blask: jasny rdzen, delikatnie zanikajace brzegi
    float glow = pow(1.0 - d, 2.2);
    vec3 core = mix(vColor, vec3(1.0), 0.6 * glow);

    FragColor = vec4(core * glow, glow * vAlpha);
}
)GLSL";

// ---------------------------------------------------------------------------
// Generowanie proceduralnej tekstury "ziemi" (bez pliku graficznego)
// ---------------------------------------------------------------------------
static float hash(int x, int y, unsigned seed) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + seed * 2654435761u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return (h & 0xFFFFFF) / float(0xFFFFFF);
}

static float smoothNoise(float x, float y, unsigned seed) {
    int x0 = (int)floor(x), y0 = (int)floor(y);
    int x1 = x0 + 1,        y1 = y0 + 1;
    float sx = x - x0, sy = y - y0;

    float n00 = hash(x0, y0, seed);
    float n10 = hash(x1, y0, seed);
    float n01 = hash(x0, y1, seed);
    float n11 = hash(x1, y1, seed);

    float ix0 = n00 + (n10 - n00) * (sx * sx * (3 - 2 * sx));
    float ix1 = n01 + (n11 - n01) * (sx * sx * (3 - 2 * sx));
    return ix0 + (ix1 - ix0) * (sy * sy * (3 - 2 * sy));
}

static GLuint createGroundTexture(int size = 256) {
    std::vector<unsigned char> pixels(size * size * 3);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float nx = x / float(size);
            float ny = y / float(size);

            // kilka oktaw szumu, zeby uzyskac plamiasta ziemie/trawe
            float n =  smoothNoise(nx * 8.0f,  ny * 8.0f,  1) * 0.5f
                     + smoothNoise(nx * 16.0f, ny * 16.0f, 2) * 0.3f
                     + smoothNoise(nx * 32.0f, ny * 32.0f, 3) * 0.2f;

            // baza: ciemna ziemia -> zielonkawa trawa
            glm::vec3 dirt  (0.30f, 0.20f, 0.10f);
            glm::vec3 grass (0.20f, 0.35f, 0.12f);
            glm::vec3 grassLight(0.32f, 0.48f, 0.18f);

            glm::vec3 color = glm::mix(dirt, grass, glm::clamp(n * 1.4f, 0.0f, 1.0f));
            color = glm::mix(color, grassLight, glm::smoothstep(0.65f, 0.9f, n));

            // drobne kamyki/ciemniejsze plamki
            float speck = smoothNoise(nx * 64.0f, ny * 64.0f, 7);
            if (speck > 0.85f) color *= 0.6f;

            int idx = (y * size + x) * 3;
            pixels[idx + 0] = (unsigned char)glm::clamp(color.r * 255.0f, 0.0f, 255.0f);
            pixels[idx + 1] = (unsigned char)glm::clamp(color.g * 255.0f, 0.0f, 255.0f);
            pixels[idx + 2] = (unsigned char)glm::clamp(color.b * 255.0f, 0.0f, 255.0f);
        }
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

// ---------------------------------------------------------------------------
// Geometria: podloga (plaska plaszczyzna z UV powtarzanym kilkukrotnie)
// ---------------------------------------------------------------------------
struct Vertex { glm::vec3 pos; glm::vec3 normal; glm::vec2 uv; };

static void createGroundMesh(GLuint& vao, GLuint& vbo, GLuint& ebo, int& indexCount) {
    float halfSize = 10.0f;
    float uvRepeat = 8.0f;

    Vertex verts[4] = {
        { {-halfSize, 0.0f, -halfSize}, {0,1,0}, {0.0f, 0.0f} },
        { { halfSize, 0.0f, -halfSize}, {0,1,0}, {uvRepeat, 0.0f} },
        { { halfSize, 0.0f,  halfSize}, {0,1,0}, {uvRepeat, uvRepeat} },
        { {-halfSize, 0.0f,  halfSize}, {0,1,0}, {0.0f, uvRepeat} },
    };
    unsigned indices[6] = { 0, 1, 2, 0, 2, 3 };
    indexCount = 6;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Geometria: butelka generowana metoda "lathe" (obrot profilu wokol osi Y)
// ---------------------------------------------------------------------------
struct SimpleVertex { glm::vec3 pos; glm::vec3 normal; };

static void createBottleMesh(GLuint& vao, GLuint& vbo, GLuint& ebo, int& indexCount) {
    // Profil butelki: (promien, wysokosc) od dolu do gory
    std::vector<glm::vec2> profile = {
        {0.00f, 0.00f}, // srodek dna (do wachlarza)
        {0.32f, 0.00f}, // krawedz dna
        {0.34f, 0.03f},
        {0.34f, 0.55f}, // korpus
        {0.30f, 0.62f}, // poczatek ramion
        {0.20f, 0.72f},
        {0.11f, 0.80f},
        {0.09f, 0.85f}, // szyjka
        {0.09f, 1.05f},
        {0.115f,1.08f}, // wyloty korka
        {0.10f, 1.10f},
        {0.00f, 1.10f}  // srodek gory (do wachlarza)
    };

    const int segments = 32;
    std::vector<SimpleVertex> vertices;
    std::vector<unsigned> indices;

    int profCount = (int)profile.size();

    // Wierzcholki: dla kazdego segmentu obrotu i kazdego punktu profilu
    for (int s = 0; s <= segments; ++s) {
        float theta = (float)s / segments * glm::two_pi<float>();
        float ct = cos(theta), st = sin(theta);

        for (int p = 0; p < profCount; ++p) {
            float r = profile[p].x;
            float y = profile[p].y;

            glm::vec3 pos(r * ct, y, r * st);

            // przyblizona normalna: nachylenie profilu + kierunek promieniowy
            glm::vec3 normal;
            if (p == 0) normal = glm::vec3(0.0f, -1.0f, 0.0f);           // dno
            else if (p == profCount - 1) normal = glm::vec3(0.0f, 1.0f, 0.0f); // gora
            else {
                glm::vec2 d = profile[p + 1] - profile[p - 1];
                glm::vec2 n2 = glm::normalize(glm::vec2(d.y, -d.x)); // prostopadla w plaszczyznie (r,y)
                normal = glm::normalize(glm::vec3(n2.x * ct, n2.y, n2.x * st));
            }

            vertices.push_back({ pos, normal });
        }
    }

    // Indeksy: laczymy sasiednie "pierscienie" profilu w trojkaty
    for (int s = 0; s < segments; ++s) {
        for (int p = 0; p < profCount - 1; ++p) {
            unsigned i0 = s * profCount + p;
            unsigned i1 = (s + 1) * profCount + p;
            unsigned i2 = (s + 1) * profCount + (p + 1);
            unsigned i3 = s * profCount + (p + 1);

            indices.push_back(i0); indices.push_back(i1); indices.push_back(i2);
            indices.push_back(i0); indices.push_back(i2); indices.push_back(i3);
        }
    }

    indexCount = (int)indices.size();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SimpleVertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void*)offsetof(SimpleVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void*)offsetof(SimpleVertex, normal));

    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Definicja efektu czastek w stylu ZenGin (silnik Gothic 1/2) - C_PARTICLEFX
// ---------------------------------------------------------------------------
// Oryginalny silnik Gothica opisuje kazdy efekt PFX jako "instancje" w
// skryptach Daedalusa, dziedziczace po klasie C_PARTICLEFX, np.
// (na podstawie publicznie znanej dokumentacji modderskiej ZenGin/PFX,
// odtworzone z pamieci - nazwy pol moga sie nieco roznic od oryginalnych
// plikow silnika, ale ich UKLAD i ZNACZENIE jest wiernie odwzorowane):
//
//   instance PFX_MAGIC_AURA_BOTTLE (C_PARTICLEFX)
//   {
//       ppsValue          = 28;          // czastek generowanych na sekunde
//       ppsIsLooping      = TRUE;
//
//       shpType           = "SPHERE";    // ksztalt emitera
//       shpFOR            = "OBJECT";    // ukl. odniesienia: obiekt czy swiat
//       shpDim            = "28 28 28";  // wymiary ksztaltu (cm w oryg. silniku)
//       shpOffsetVec      = "0 90 0";    // przesuniecie wzgledem obiektu
//       shpIsVolume       = FALSE;       // FALSE = emisja z "powloki" ksztaltu
//
//       dirMode           = "DIR";       // tryb kierunku wylotu czastki
//       dirAngleHead      = 0;           // kat azymutu (glowny)
//       dirAngleHeadVar   = 180;         // losowa wariacja azymutu
//       dirAngleElev      = -80;         // kat elewacji (-90 = pionowo w gore)
//       dirAngleElevVar   = 20;
//
//       velAvg            = 30;          // predkosc poczatkowa (srednia)
//       velVar            = 12;          // wariacja predkosci
//
//       lspPartAvg        = 1.6;         // czas zycia czastki (srednia, s)
//       lspPartVar        = 0.5;         // wariacja czasu zycia
//
//       flyGravity        = "0 5 0";     // "grawitacja" dzialajaca na czastke
//
//       visSizeStart      = "4.5 4.5";   // rozmiar startowy
//       visSizeEndScale   = 0.25;        // mnoznik rozmiaru na koncu zycia
//       visAlphaFunc      = "ADD";       // tryb mieszania (ADD = swiecenie)
//       visAlphaStart     = 255;         // przezroczystosc na starcie (0-255)
//       visAlphaEnd       = 0;           // przezroczystosc na koncu (0-255)
//       visTexColorStart  = "255 225 130"; // kolor na starcie zycia (RGB 0-255)
//       visTexColorEnd    = "150 210 255"; // kolor na koncu zycia (RGB 0-255)
//   };
//
// Ponizsza struktura C++ mapuje 1:1 powyzsze pola i steruje naszym systemem
// czastek dokladnie w ten sam sposob, w jaki sterowalby nim oryginalny
// silnik ZenGin (ciagla emisja "particles per second", losowy kierunek
// wg kata azymutu/elewacji + wariancji, predkosc/czas zycia z pary
// srednia+wariancja, interpolacja rozmiaru/alfa/koloru w trakcie zycia
// czastki, a nie z gory ustalona orbita jak w poprzedniej wersji).
// ---------------------------------------------------------------------------

enum class ShpType { POINT, SPHERE, CIRCLE, BOX };   // shpType
enum class ShpFOR  { OBJECT, WORLD };                 // shpFOR
enum class DirMode { NONE, DIR };                     // dirMode
enum class AlphaFunc { BLEND, ADD };                  // visAlphaFunc

struct ParticleFxDef {
    // --- emisja (pps = "particles per second") ---
    float ppsValue      = 20.0f;
    bool  ppsIsLooping   = true;

    // --- ksztalt emitera (shp = "shape") ---
    ShpType   shpType      = ShpType::SPHERE;
    ShpFOR    shpFOR       = ShpFOR::OBJECT;
    glm::vec3 shpDim       = glm::vec3(0.28f);
    glm::vec3 shpOffsetVec = glm::vec3(0.0f, 0.9f, 0.0f);
    bool      shpIsVolume  = false; // false = emisja z powierzchni ksztaltu

    // --- kierunek wylotu czastki (dir) ---
    DirMode dirMode         = DirMode::DIR;
    float   dirAngleHead    = 0.0f;
    float   dirAngleHeadVar = 180.0f;
    float   dirAngleElev    = -80.0f;
    float   dirAngleElevVar = 20.0f;

    // --- predkosc poczatkowa (vel) ---
    float velAvg = 0.30f;
    float velVar = 0.12f;

    // --- czas zycia pojedynczej czastki (lsp = "lifespan particle") ---
    float lspPartAvg = 1.6f;
    float lspPartVar = 0.5f;

    // --- "grawitacja" dzialajaca na predkosc czastki (fly) ---
    glm::vec3 flyGravity = glm::vec3(0.0f, 0.05f, 0.0f);

    // --- wyglad (vis) ---
    glm::vec2 visSizeStart    = glm::vec2(0.045f);
    float     visSizeEndScale = 0.25f;
    AlphaFunc visAlphaFunc    = AlphaFunc::ADD;
    float     visAlphaStart   = 255.0f; // skala 0-255, jak w ZenGin
    float     visAlphaEnd     = 0.0f;
    glm::vec3 visTexColorStart = glm::vec3(255, 225, 130); // RGB 0-255
    glm::vec3 visTexColorEnd   = glm::vec3(150, 210, 255); // RGB 0-255
};

// Konkretna "instancja" efektu - odpowiednik instance PFX_... z Daedalusa.
static ParticleFxDef makePfxMagicAuraBottle() {
    ParticleFxDef fx;
    fx.ppsValue        = 28.0f;
    fx.ppsIsLooping     = true;

    fx.shpType         = ShpType::SPHERE;
    fx.shpFOR          = ShpFOR::OBJECT;
    fx.shpDim          = glm::vec3(0.28f);
    fx.shpOffsetVec    = glm::vec3(0.0f, 0.9f, 0.0f);
    fx.shpIsVolume     = false;

    fx.dirMode         = DirMode::DIR;
    fx.dirAngleHead    = 0.0f;
    fx.dirAngleHeadVar = 180.0f;
    fx.dirAngleElev    = -80.0f;
    fx.dirAngleElevVar = 20.0f;

    fx.velAvg = 0.30f;
    fx.velVar = 0.12f;

    fx.lspPartAvg = 1.6f;
    fx.lspPartVar = 0.5f;

    fx.flyGravity = glm::vec3(0.0f, 0.05f, 0.0f);

    fx.visSizeStart     = glm::vec2(0.045f);
    fx.visSizeEndScale  = 0.25f;
    fx.visAlphaFunc     = AlphaFunc::ADD;
    fx.visAlphaStart    = 255.0f;
    fx.visAlphaEnd      = 0.0f;
    fx.visTexColorStart = glm::vec3(255, 225, 130);
    fx.visTexColorEnd   = glm::vec3(150, 210, 255);
    return fx;
}

// ---------------------------------------------------------------------------
// Runtime pojedynczej czastki (odpowiednik zCParticle w ZenGin)
// ---------------------------------------------------------------------------
struct ParticleInstance {
    bool      active  = false;
    glm::vec3 pos     = glm::vec3(0.0f);
    glm::vec3 vel     = glm::vec3(0.0f);
    float     life    = 0.0f; // uplyniety czas zycia
    float     maxLife = 1.0f;
};

// ---------------------------------------------------------------------------
// Silnik efektu - odpowiednik zCParticleFX / emitera z ZenGin. Steruje sie
// nim wylacznie przez ParticleFxDef, dokladnie tak jak w oryginale, gdzie
// caly wyglad i zachowanie efektu ustawia sie w skrypcie (pliku PFX), a nie
// w kodzie C++.
// ---------------------------------------------------------------------------
class ZenParticleFX {
public:
    void init(const ParticleFxDef& def, glm::vec3 emitterPos, int maxParticles = 0) {
        m_def = def;
        m_emitterPos = emitterPos;
        m_rng.seed(1337);

        if (maxParticles <= 0) {
            // pula wyliczona z tempa emisji i maksymalnego czasu zycia,
            // analogicznie jak silnik szacuje potrzebny rozmiar puli
            float maxLife = m_def.lspPartAvg + m_def.lspPartVar;
            maxParticles = int(std::ceil(m_def.ppsValue * maxLife)) + 16;
        }
        m_particles.resize(maxParticles);

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        // pos(3) + alpha(1) + size(1) + color(3) = 8 floatow na czastke
        glBufferData(GL_ARRAY_BUFFER, m_particles.size() * 8 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));

        glBindVertexArray(0);
    }

    void update(float dt, glm::vec3 emitterPos) {
        m_emitterPos = emitterPos;

        // --- emisja: ppsValue czastek na sekunde (ciagla emisja jak w ZenGin) ---
        if (m_def.ppsIsLooping) {
            m_spawnAccumulator += m_def.ppsValue * dt;
            while (m_spawnAccumulator >= 1.0f) {
                spawnOne();
                m_spawnAccumulator -= 1.0f;
            }
        }

        m_buffer.clear();
        m_buffer.reserve(m_particles.size() * 8);

        for (auto& pt : m_particles) {
            if (pt.active) {
                pt.vel += m_def.flyGravity * dt;
                pt.pos += pt.vel * dt;
                pt.life += dt;
                if (pt.life >= pt.maxLife) pt.active = false;
            }

            if (!pt.active) {
                // nieaktywna czastka - zerowa alfa, niewidoczna, ale nadal
                // wysylana do bufora (staly rozmiar draw call jak w ZenGin
                // pool czastek)
                m_buffer.push_back(pt.pos.x);
                m_buffer.push_back(pt.pos.y);
                m_buffer.push_back(pt.pos.z);
                m_buffer.push_back(0.0f); // alpha
                m_buffer.push_back(0.0f); // size
                m_buffer.push_back(0.0f);
                m_buffer.push_back(0.0f);
                m_buffer.push_back(0.0f);
                continue;
            }

            float t = glm::clamp(pt.life / pt.maxLife, 0.0f, 1.0f);

            float alpha = glm::mix(m_def.visAlphaStart, m_def.visAlphaEnd, t) / 255.0f;
            float size  = glm::mix(m_def.visSizeStart.x, m_def.visSizeStart.x * m_def.visSizeEndScale, t);
            glm::vec3 color = glm::mix(m_def.visTexColorStart, m_def.visTexColorEnd, t) / 255.0f;

            m_buffer.push_back(pt.pos.x);
            m_buffer.push_back(pt.pos.y);
            m_buffer.push_back(pt.pos.z);
            m_buffer.push_back(alpha);
            m_buffer.push_back(size * 60.0f); // przeskalowanie do rozmiaru w pikselach point sprite'a
            m_buffer.push_back(color.r);
            m_buffer.push_back(color.g);
            m_buffer.push_back(color.b);
        }

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_buffer.size() * sizeof(float), m_buffer.data());
    }

    void draw(GLuint program, const glm::mat4& view, const glm::mat4& proj) {
        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(program, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));

        glEnable(GL_PROGRAM_POINT_SIZE);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);

        // visAlphaFunc steruje trybem mieszania, dokladnie jak w ZenGin:
        // ADD -> addytywne swiecenie, BLEND -> zwykla przezroczystosc
        if (m_def.visAlphaFunc == AlphaFunc::ADD)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        else
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindVertexArray(m_vao);
        glDrawArrays(GL_POINTS, 0, (GLsizei)m_particles.size());
        glBindVertexArray(0);

        glDepthMask(GL_TRUE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

private:
    void spawnOne() {
        // szukamy wolnego miejsca w puli (jak recykling czastek w ZenGin)
        for (auto& pt : m_particles) {
            if (pt.active) continue;

            pt.active  = true;
            pt.life    = 0.0f;
            pt.maxLife = std::max(0.05f, sampleAvgVar(m_def.lspPartAvg, m_def.lspPartVar));

            glm::vec3 localOffset = sampleShapeOffset();
            glm::vec3 basePos = (m_def.shpFOR == ShpFOR::OBJECT) ? m_emitterPos : glm::vec3(0.0f);
            pt.pos = basePos + localOffset;

            glm::vec3 dir = sampleDirection();
            float speed = std::max(0.0f, sampleAvgVar(m_def.velAvg, m_def.velVar));
            pt.vel = dir * speed;
            return;
        }
        // brak wolnego miejsca - pomijamy ten "tick" emisji (jak przy
        // przekroczeniu limitu czastek w oryginalnym silniku)
    }

    float sampleAvgVar(float avg, float var) {
        std::uniform_real_distribution<float> d(-1.0f, 1.0f);
        return avg + d(m_rng) * var;
    }

    glm::vec3 sampleShapeOffset() {
        std::uniform_real_distribution<float> u(-1.0f, 1.0f);

        switch (m_def.shpType) {
            case ShpType::POINT:
                return m_def.shpOffsetVec;

            case ShpType::SPHERE: {
                glm::vec3 dir = glm::normalize(glm::vec3(u(m_rng), u(m_rng), u(m_rng)));
                float r = m_def.shpIsVolume ? std::cbrt(std::abs(u(m_rng))) : 1.0f;
                return m_def.shpOffsetVec + dir * m_def.shpDim.x * r;
            }
            case ShpType::CIRCLE: {
                float a = u(m_rng) * glm::pi<float>();
                float r = m_def.shpIsVolume ? std::sqrt(std::abs(u(m_rng))) : 1.0f;
                return m_def.shpOffsetVec + glm::vec3(cos(a), 0.0f, sin(a)) * m_def.shpDim.x * r;
            }
            case ShpType::BOX:
            default:
                return m_def.shpOffsetVec + glm::vec3(u(m_rng), u(m_rng), u(m_rng)) * m_def.shpDim;
        }
    }

    glm::vec3 sampleDirection() {
        std::uniform_real_distribution<float> u(-1.0f, 1.0f);

        if (m_def.dirMode == DirMode::NONE)
            return glm::normalize(glm::vec3(u(m_rng), u(m_rng), u(m_rng)));

        float head = glm::radians(m_def.dirAngleHead + u(m_rng) * m_def.dirAngleHeadVar);
        float elev = glm::radians(m_def.dirAngleElev + u(m_rng) * m_def.dirAngleElevVar);

        // elev = -90 stopni oznacza pionowo w gore (+Y), zgodnie z konwencja
        // katow elewacji uzywana w definicji efektu powyzej
        float ce = cos(elev), se = sin(elev);
        return glm::normalize(glm::vec3(ce * cos(head), -se, ce * sin(head)));
    }

    ParticleFxDef m_def;
    std::vector<ParticleInstance> m_particles;
    std::vector<float> m_buffer;
    glm::vec3 m_emitterPos;
    float m_spawnAccumulator = 0.0f;
    std::mt19937 m_rng;
    GLuint m_vao = 0, m_vbo = 0;
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    if (!glfwInit()) {
        std::cerr << "Nie udalo sie zainicjalizowac GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4); // MSAA

    GLFWwindow* window = glfwCreateWindow(g_windowWidth, g_windowHeight,
        "Gothic-style Bottle + Magic Aura (epoxy)", nullptr, nullptr);
    if (!window) {
        std::cerr << "Nie udalo sie utworzyc okna GLFW\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // Uwaga: w przeciwienstwie do GLAD, epoxy NIE wymaga jawnej inicjalizacji
    // loadera (np. gladLoadGLLoader). Wystarczy aktywny kontekst OpenGL -
    // epoxy sam rozwiazuje wskazniki funkcji przy pierwszym wywolaniu.

    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);

    std::cout << "OpenGL (epoxy) wersja: " << glGetString(GL_VERSION) << std::endl;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // --- shadery ---
    GLuint groundProgram   = linkProgram(groundVS, groundFS);
    GLuint bottleProgram   = linkProgram(bottleVS, bottleFS);
    GLuint particleProgram = linkProgram(particleVS, particleFS);

    // --- geometria ---
    GLuint groundVAO, groundVBO, groundEBO; int groundIndexCount;
    createGroundMesh(groundVAO, groundVBO, groundEBO, groundIndexCount);
    GLuint groundTex = createGroundTexture(256);

    GLuint bottleVAO, bottleVBO, bottleEBO; int bottleIndexCount;
    createBottleMesh(bottleVAO, bottleVBO, bottleEBO, bottleIndexCount);

    glm::vec3 bottlePos(0.0f, 0.0f, 0.0f);
    glm::vec3 bottleColor(0.05f, 0.35f, 0.12f); // szklo w kolorze butelkowej zieleni

    // Efekt "aury" ladowany jest z definicji PFX_MAGIC_AURA_BOTTLE - dokladnie
    // tak, jak w silniku Gothica wczytuje sie efekt czastek ze skryptu PFX
    // podpietego pod dany obiekt (tu: butelke).
    ZenParticleFX aura;
    aura.init(makePfxMagicAuraBottle(), bottlePos);

    glm::vec3 lightDir = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));

    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window)) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        dt = glm::min(dt, 0.05f); // zabezpieczenie przed duzymi skokami dt

        processInput(window, dt);
        aura.update(dt, bottlePos);

        glClearColor(0.06f, 0.08f, 0.10f, 1.0f); // ciemny, "dusk-owy" klimat pod aure
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 proj = glm::perspective(glm::radians(g_camera.fov),
            float(g_windowWidth) / float(g_windowHeight), 0.05f, 100.0f);
        glm::mat4 view = g_camera.view();

        // --- podloga ---
        glUseProgram(groundProgram);
        glm::mat4 groundModel = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(groundProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(groundModel));
        glUniformMatrix4fv(glGetUniformLocation(groundProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(groundProgram, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniform3fv(glGetUniformLocation(groundProgram, "uLightDir"), 1, glm::value_ptr(lightDir));
        glUniform3fv(glGetUniformLocation(groundProgram, "uViewPos"), 1, glm::value_ptr(g_camera.position));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, groundTex);
        glUniform1i(glGetUniformLocation(groundProgram, "uGroundTex"), 0);

        glBindVertexArray(groundVAO);
        glDrawElements(GL_TRIANGLES, groundIndexCount, GL_UNSIGNED_INT, 0);

        // --- butelka ---
        glUseProgram(bottleProgram);
        glm::mat4 bottleModel = glm::translate(glm::mat4(1.0f), bottlePos);
        glUniformMatrix4fv(glGetUniformLocation(bottleProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(bottleModel));
        glUniformMatrix4fv(glGetUniformLocation(bottleProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(bottleProgram, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniform3fv(glGetUniformLocation(bottleProgram, "uBaseColor"), 1, glm::value_ptr(bottleColor));
        glUniform3fv(glGetUniformLocation(bottleProgram, "uLightDir"), 1, glm::value_ptr(lightDir));
        glUniform3fv(glGetUniformLocation(bottleProgram, "uViewPos"), 1, glm::value_ptr(g_camera.position));

        glBindVertexArray(bottleVAO);
        glDrawElements(GL_TRIANGLES, bottleIndexCount, GL_UNSIGNED_INT, 0);

        // --- aura pfx (rysowana na koncu, additive blending) ---
        aura.draw(particleProgram, view, proj);

        glBindVertexArray(0);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
