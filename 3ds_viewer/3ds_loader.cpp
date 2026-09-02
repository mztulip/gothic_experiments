#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "texture_loader.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

enum CameraMode
{
    CAMERA_ORBIT = 0,
    CAMERA_FPS = 1
};

enum RenderMode
{
    RENDER_TEXTURED = 0,
    RENDER_SOLID_COLOR = 1,
    RENDER_WIREFRAME = 2
};

struct Vertex
{
    glm::vec3 pos;
    glm::vec2 uv;
    glm::vec3 normal;
};

struct Face
{
    uint16_t a, b, c;
};

struct Mesh3DS
{
    std::vector<Vertex> vertices;
    std::vector<Face> faces;
    std::string textureFile;
    
    // Bounding Box modelu
    glm::vec3 minBounds{0.0f};
    glm::vec3 maxBounds{0.0f};
    glm::vec3 center{0.0f};
    float maxDimension = 1.0f;
};

class Loader3DS
{
public:
    static bool load(const std::string& filepath, Mesh3DS& outMesh)
    {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open())
        {
            return false;
        }

        outMesh.vertices.clear();
        outMesh.faces.clear();
        outMesh.textureFile = "";

        file.seekg(0, std::ios::end);
        uint32_t fileSize = static_cast<uint32_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        parseChunk(file, fileSize, outMesh);
        calculateNormals(outMesh);
        calculateBounds(outMesh);

        return !outMesh.vertices.empty();
    }

private:
    static void calculateBounds(Mesh3DS& mesh)
    {
        if (mesh.vertices.empty()) return;

        mesh.minBounds = mesh.vertices[0].pos;
        mesh.maxBounds = mesh.vertices[0].pos;

        for (const auto& v : mesh.vertices)
        {
            mesh.minBounds = glm::min(mesh.minBounds, v.pos);
            mesh.maxBounds = glm::max(mesh.maxBounds, v.pos);
        }

        mesh.center = (mesh.minBounds + mesh.maxBounds) * 0.5f;
        glm::vec3 size = mesh.maxBounds - mesh.minBounds;
        mesh.maxDimension = std::max({size.x, size.y, size.z});
        if (mesh.maxDimension <= 0.0001f) mesh.maxDimension = 1.0f;
    }

    static void calculateNormals(Mesh3DS& mesh)
    {
        for (auto& v : mesh.vertices)
        {
            v.normal = glm::vec3(0.0f);
        }

        for (const auto& face : mesh.faces)
        {
            glm::vec3 v0 = mesh.vertices[face.a].pos;
            glm::vec3 v1 = mesh.vertices[face.b].pos;
            glm::vec3 v2 = mesh.vertices[face.c].pos;

            glm::vec3 norm = glm::normalize(glm::cross(v1 - v0, v2 - v0));

            mesh.vertices[face.a].normal += norm;
            mesh.vertices[face.b].normal += norm;
            mesh.vertices[face.c].normal += norm;
        }

        for (auto& v : mesh.vertices)
        {
            if (glm::length(v.normal) > 0.0001f)
            {
                v.normal = glm::normalize(v.normal);
            }
            else
            {
                v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }
    }

    static void parseChunk(std::ifstream& file, uint32_t endPos, Mesh3DS& mesh)
    {
        while (file.tellg() < endPos && file.good())
        {
            uint16_t chunkId;
            uint32_t chunkLength;

            file.read(reinterpret_cast<char*>(&chunkId), sizeof(chunkId));
            file.read(reinterpret_cast<char*>(&chunkLength), sizeof(chunkLength));

            uint32_t nextChunk = static_cast<uint32_t>(file.tellg()) + chunkLength - 6;

            switch (chunkId)
            {
                case 0x4D4D:
                case 0x3D3D:
                case 0x4000:
                case 0xAFFF:
                case 0xA200:
                {
                    if (chunkId == 0x4000)
                    {
                        char ch;
                        while (file.get(ch) && ch != '\0');
                    }
                    parseChunk(file, nextChunk, mesh);
                    break;
                }
                case 0x4100:
                {
                    parseChunk(file, nextChunk, mesh);
                    break;
                }
                case 0x4110:
                {
                    uint16_t numVertices;
                    file.read(reinterpret_cast<char*>(&numVertices), sizeof(numVertices));
                    mesh.vertices.resize(numVertices);
                    for (int i = 0; i < numVertices; ++i)
                    {
                        file.read(reinterpret_cast<char*>(&mesh.vertices[i].pos), sizeof(glm::vec3));
                        mesh.vertices[i].uv = glm::vec2(0.0f, 0.0f);
                        mesh.vertices[i].normal = glm::vec3(0.0f);
                    }
                    break;
                }
                case 0x4140:
                {
                    uint16_t numUVs;
                    file.read(reinterpret_cast<char*>(&numUVs), sizeof(numUVs));
                    if (numUVs == mesh.vertices.size())
                    {
                        for (int i = 0; i < numUVs; ++i)
                        {
                            file.read(reinterpret_cast<char*>(&mesh.vertices[i].uv), sizeof(glm::vec2));
                        }
                    }
                    else
                    {
                        file.seekg(nextChunk, std::ios::beg);
                    }
                    break;
                }
                case 0x4120:
                {
                    uint16_t numFaces;
                    file.read(reinterpret_cast<char*>(&numFaces), sizeof(numFaces));
                    mesh.faces.resize(numFaces);
                    for (int i = 0; i < numFaces; ++i)
                    {
                        file.read(reinterpret_cast<char*>(&mesh.faces[i]), 3 * sizeof(uint16_t));
                        uint16_t flags;
                        file.read(reinterpret_cast<char*>(&flags), sizeof(flags));
                    }
                    break;
                }
                case 0xA300:
                {
                    char ch;
                    std::string texName = "";
                    while (file.get(ch) && ch != '\0')
                    {
                        texName += ch;
                    }
                    mesh.textureFile = texName;
                    file.seekg(nextChunk, std::ios::beg);
                    break;
                }
                default:
                {
                    file.seekg(nextChunk, std::ios::beg);
                    break;
                }
            }
        }
    }
};

// Stan aplikacji
static float g_yaw = 0.0f;
static float g_pitch = 15.0f;
static float g_distance = 200.0f;
static bool g_keys[512] = {};
static double g_lastInteractionTime = 0.0;

static bool g_isMouseDown = false;
static double g_lastMouseX = 0.0;
static double g_lastMouseY = 0.0;

static std::vector<std::string> g_fileList;
static size_t g_currentFileIdx = 0;
static bool g_needMeshReload = false;
static bool g_scrollToSelected = false;
static bool g_enableAlphaTest = true; // Flaga włączania przezroczystości
static bool g_showAlphaBounds = false;  // Podświetlanie struktury płacht

static RenderMode g_renderMode = RENDER_TEXTURED;
static bool g_showHUD = true;
static bool g_enableLighting = true;

// Ustawienia Światła
static float g_lightAngle = 45.0f;        // Kąt w stopniach (0 - 360)
static float g_lightIntensity = 1.4f;     // Moc oświetlenia głównego (Diffuse)
static float g_ambientIntensity = 0.25f;  // Jasność tła/cieni (Ambient)
static float g_lightDistanceMult = 0.75f; // Dystans od środka obiektu
static float g_lightHeightOffset = 0.5f;  // Wysokość ponad najwyższy punkt obiektu

static CameraMode g_cameraMode = CAMERA_ORBIT;
static glm::vec3 g_fpsCameraPos = glm::vec3(0.0f, 100.0f, 200.0f);
static float g_flySpeed = 500.0f; // Domyślna prędkość latania
static bool g_toggleCameraRequested = false;

void switchCameraMode(CameraMode newMode, const glm::vec3& rotatedCenter)
{
    if (g_cameraMode == newMode) return;

    float radYaw = glm::radians(g_yaw);
    float radPitch = glm::radians(g_pitch);

    if (newMode == CAMERA_FPS)
    {
        // 1. Obliczamy aktualną pozycję kamery z trybu Orbit
        g_fpsCameraPos.x = rotatedCenter.x + g_distance * cos(radPitch) * sin(radYaw);
        g_fpsCameraPos.y = rotatedCenter.y + g_distance * sin(radPitch);
        g_fpsCameraPos.z = rotatedCenter.z + g_distance * cos(radPitch) * cos(radYaw);

        // 2. Obliczamy kierunek od kamery do środka obiektu
        glm::vec3 dir = glm::normalize(rotatedCenter - g_fpsCameraPos);

        // 3. Przeliczamy g_yaw i g_pitch tak, aby kamera FPS patrzyła na obiekt
        g_pitch = glm::degrees(asin(dir.y));
        g_yaw   = glm::degrees(atan2(dir.x, dir.z));
    }
    else // Odwrót: przesiadka z FPS na Orbit
    {
        // Ustawiamy dystans jako odległość od środka obiektu
        g_distance = glm::distance(g_fpsCameraPos, rotatedCenter);
        g_distance = std::clamp(g_distance, 1.0f, 500000.0f);

        // Obliczamy wektor od środka obiektu do kamery
        glm::vec3 dir = glm::normalize(g_fpsCameraPos - rotatedCenter);

        g_pitch = glm::degrees(asin(dir.y));
        g_yaw   = glm::degrees(atan2(dir.x, dir.z));
    }

    g_cameraMode = newMode;
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            g_isMouseDown = true;
            glfwGetCursorPos(window, &g_lastMouseX, &g_lastMouseY);
            g_lastInteractionTime = glfwGetTime();
        }
        else if (action == GLFW_RELEASE)
        {
            g_isMouseDown = false;
            g_lastInteractionTime = glfwGetTime();
        }
    }
}

static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    if (g_isMouseDown)
    {
        float dx = static_cast<float>(xpos - g_lastMouseX);
        float dy = static_cast<float>(ypos - g_lastMouseY);

        float sensitivity = 0.2f;

        if (g_cameraMode == CAMERA_ORBIT)
        {
            g_yaw += dx * sensitivity;
            g_pitch -= dy * sensitivity;
        }
        else // CAMERA_FPS
        {
            g_yaw += dx * sensitivity;
            g_pitch += dy * sensitivity; // Zmiana na '+' dla naturalnej osi Y w FPS
        }

        g_pitch = std::clamp(g_pitch, -89.0f, 89.0f);

        g_lastMouseX = xpos;
        g_lastMouseY = ypos;
        g_lastInteractionTime = glfwGetTime();
    }
}

static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    if (g_cameraMode == CAMERA_ORBIT)
    {
        g_distance -= static_cast<float>(yoffset) * (g_distance * 0.1f);
        g_distance = std::clamp(g_distance, 1.0f, 50000.0f);
    }
    else // CAMERA_FPS – regulacja prędkości latania
    {
        g_flySpeed += static_cast<float>(yoffset) * (g_flySpeed * 0.2f);
        g_flySpeed = std::clamp(g_flySpeed, 10.0f, 100000.0f);
    }

    g_lastInteractionTime = glfwGetTime();
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    // if (io.WantCaptureKeyboard) return;

    if (key >= 0 && key < 512)
    {
        if (action == GLFW_PRESS)   g_keys[key] = true;
        if (action == GLFW_RELEASE) g_keys[key] = false;
    }

    if (action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        if (key == GLFW_KEY_M)
        {
            g_renderMode = static_cast<RenderMode>((static_cast<int>(g_renderMode) + 1) % 3);
        }

        if (key == GLFW_KEY_T) g_showHUD = !g_showHUD;
        if (key == GLFW_KEY_L) g_enableLighting = !g_enableLighting;

        if (!g_fileList.empty())
        {
            if (key == GLFW_KEY_DOWN || key == GLFW_KEY_N)
            {
                g_currentFileIdx = (g_currentFileIdx + 1) % g_fileList.size();
                g_needMeshReload = true;
                g_scrollToSelected = true;
            }
            if (key == GLFW_KEY_UP || key == GLFW_KEY_P)
            {
                g_currentFileIdx = (g_currentFileIdx + g_fileList.size() - 1) % g_fileList.size();
                g_needMeshReload = true;
                g_scrollToSelected = true;
            }
        }

        if (key == GLFW_KEY_C)
        {
            g_toggleCameraRequested = true;
        }

        if (key == GLFW_KEY_O)
        {
            g_enableAlphaTest = !g_enableAlphaTest;
        }
    }
}

void scanGothicDirectory(const std::string& gothicDir)
{
    fs::path targetPath = fs::path(gothicDir) / "_Work" / "Data" / "Meshes";
    if (!fs::exists(targetPath))
    {
        targetPath = gothicDir;
    }

    try
    {
        for (const auto& entry : fs::recursive_directory_iterator(targetPath))
        {
            if (entry.is_regular_file())
            {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".3ds")
                {
                    g_fileList.push_back(entry.path().string());
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Blad skanowania: " << e.what() << std::endl;
    }

    std::sort(g_fileList.begin(), g_fileList.end());
}

Texture2D createFallbackTexture()
{
    Texture2D tex;
    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);

    uint32_t pixels[16 * 16];
    for (int y = 0; y < 16; ++y)
    {
        for (int x = 0; x < 16; ++x)
        {
            bool check = ((x / 2) + (y / 2)) % 2 == 0;
            pixels[y * 16 + x] = check ? 0xFFFFFFFF : 0xFF808080;
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    tex.width = 16;
    tex.height = 16;
    tex.valid = true;
    return tex;
}

static const char* MESH_VERT_SRC = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

out vec2 TexCoord;
out vec3 FragNormal;
out vec3 FragPos;

uniform mat4 MVP;
uniform mat4 Model;

void main() 
{
    gl_Position = MVP * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
    FragNormal = mat3(transpose(inverse(Model))) * aNormal;
    FragPos = vec3(Model * vec4(aPos, 1.0));
}
)GLSL";

static const char* MESH_FRAG_SRC = R"GLSL(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 FragNormal;
in vec3 FragPos;

uniform sampler2D uTexture;
uniform int uRenderMode; // 0: Texture, 1: Solid Color, 2: Wireframe
uniform bool uEnableLighting;
uniform bool uEnableAlphaTest; // Przezroczystość
uniform bool uShowAlphaBounds; // Pokazanie niewidocznej struktury płacht
uniform vec3 uLightPos;
uniform float uLightIntensity;   
uniform float uAmbientIntensity; 

void main() 
{
    vec4 baseColor;

    if (uRenderMode == 0) 
    {
        baseColor = texture(uTexture, TexCoord);
        
        if (uEnableAlphaTest) 
        {
            // Normalny tryb przezroczystości (odrzucamy czarne tło)
            if (baseColor.a < 0.1) 
            {
                discard;
            }
        } 
        else if (uShowAlphaBounds && baseColor.a < 0.1) 
        {
            // TRYB STRUKTURY: podświetlamy tło płachty
            // TRYB STRUKTURA: Neonowa magenta/róż
            baseColor = vec4(1.0, 0.0, 0.6, 1.0);
        }
    } 
    else if (uRenderMode == 1) 
    {
        baseColor = vec4(0.7, 0.7, 0.75, 1.0);
    } 
    else 
    {
        baseColor = vec4(0.0, 0.8, 1.0, 1.0);
    }

    if (uEnableLighting && uRenderMode != 2) 
    {
        vec3 norm = normalize(FragNormal);
        vec3 lightDir = normalize(uLightPos - FragPos);
        
        vec3 ambient = uAmbientIntensity * vec3(1.0);

        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * vec3(uLightIntensity);

        vec3 result = (ambient + diffuse) * baseColor.rgb;
        FragColor = vec4(result, baseColor.a);
    } 
    else 
    {
        FragColor = baseColor;
    }
}
)GLSL";

static const char* LIGHT_VERT_SRC = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 MVP;

void main() 
{
    gl_Position = MVP * vec4(aPos, 1.0);
}
)GLSL";

static const char* LIGHT_FRAG_SRC = R"GLSL(
#version 330 core
out vec4 FragColor;

uniform vec3 uColor;

void main() 
{
    FragColor = vec4(uColor, 1.0);
}
)GLSL";

static GLuint compileShader(GLenum type, const char* src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    return sh;
}

static GLuint createProgram(const char* vSrc, const char* fSrc)
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, vSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        g_fileList.push_back(argv[1]);
    }
    else
    {
        const char* envPath = std::getenv("GOTHIC2_DIR");
        if (envPath) scanGothicDirectory(envPath);
    }

    if (g_fileList.empty())
    {
        std::cerr << "Brak plikow .3DS!" << std::endl;
        return -1;
    }

    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Gothic 3DS Viewer", nullptr, nullptr);
    if (!window) return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetKeyCallback(window, keyCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    GLuint meshVAO, meshVBO, meshEBO;
    glGenVertexArrays(1, &meshVAO);
    glGenBuffers(1, &meshVBO);
    glGenBuffers(1, &meshEBO);

    float cubeVertices[] = {
        -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f
    };

    uint16_t cubeIndices[] = {
        0, 1, 2,  2, 3, 0,
        4, 5, 6,  6, 7, 4,
        0, 1, 5,  5, 4, 0,
        2, 3, 7,  7, 6, 2,
        0, 3, 7,  7, 4, 0,
        1, 2, 6,  6, 5, 1
    };

    GLuint lightVAO, lightVBO, lightEBO;
    glGenVertexArrays(1, &lightVAO);
    glGenBuffers(1, &lightVBO);
    glGenBuffers(1, &lightEBO);

    glBindVertexArray(lightVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lightVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lightEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    GLuint meshProgram = createProgram(MESH_VERT_SRC, MESH_FRAG_SRC);
    GLuint lightProgram = createProgram(LIGHT_VERT_SRC, LIGHT_FRAG_SRC);

    Texture2D fallbackTex = createFallbackTexture();
    Texture2D activeTex = fallbackTex;

    Mesh3DS mesh;
    auto uploadMesh = [&](const std::string& path)
    {
        if (Loader3DS::load(path, mesh))
        {
            glBindVertexArray(meshVAO);

            glBindBuffer(GL_ARRAY_BUFFER, meshVBO);
            glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(Vertex), mesh.vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.faces.size() * sizeof(Face), mesh.faces.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
            glEnableVertexAttribArray(0);

            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
            glEnableVertexAttribArray(1);

            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
            glEnableVertexAttribArray(2);

            g_distance = mesh.maxDimension * 2.5f;

            g_flySpeed = std::clamp(mesh.maxDimension * 0.5f, 50.0f, 10000.0f);

            glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::vec3 rotatedCenter = glm::vec3(model * glm::vec4(mesh.center, 1.0f));
            
            g_fpsCameraPos = rotatedCenter + glm::vec3(0.0f, mesh.maxDimension * 0.2f, g_distance);

            if (activeTex.id != fallbackTex.id)
            {
                activeTex.free();
            }

            activeTex = fallbackTex;

            if (!mesh.textureFile.empty())
            {
                std::cout << "\n[3DS LOG] Model wymaga tekstury: '" << mesh.textureFile << "'" << std::endl;

                std::string resolvedPath = "";
                fs::path localTex = fs::path(path).parent_path() / mesh.textureFile;

                if (fs::exists(localTex))
                {
                    resolvedPath = localTex.string();
                }
                else
                {
                    const char* envPath = std::getenv("GOTHIC2_DIR");
                    std::string gothicDir = envPath ? envPath : fs::path(path).parent_path().string();
                    resolvedPath = TextureLoader::resolveGothicTexturePath(mesh.textureFile, gothicDir);
                }

                if (!resolvedPath.empty())
                {
                    std::cout << " -> Znaleziono plik: " << resolvedPath << std::endl;
                    Texture2D loaded = TextureLoader::loadFromFile(resolvedPath, true);
                    if (loaded.valid)
                    {
                        activeTex = loaded;
                    }
                }
                else
                {
                    std::cout << " -> BLAD: Nie odnaleziono tekstury na dysku." << std::endl;
                }
            }
        }
    };

    uploadMesh(g_fileList[g_currentFileIdx]);
    double lastTime = glfwGetTime();

    char filterBuffer[128] = "";

    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        glfwPollEvents();

        if (g_needMeshReload)
        {
            uploadMesh(g_fileList[g_currentFileIdx]);
            g_needMeshReload = false;
        }

        // --- OBSŁUGA POZYCJI KAMERY I KLAWIATURY ---
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::vec3 rotatedCenter = glm::vec3(model * glm::vec4(mesh.center, 1.0f));

        // Reakcja na klawisz C z keyCallback
        if (g_toggleCameraRequested)
        {
            std::cout << "[LOG] Zmiana trybu kamery..." << std::endl;
            CameraMode nextMode = (g_cameraMode == CAMERA_ORBIT) ? CAMERA_FPS : CAMERA_ORBIT;
            switchCameraMode(nextMode, rotatedCenter);
            g_toggleCameraRequested = false;
        }

        glm::vec3 camPos;
        glm::vec3 camFront;
        glm::vec3 camUp = glm::vec3(0.0f, 1.0f, 0.0f);

        float radYaw = glm::radians(g_yaw);
        float radPitch = glm::radians(g_pitch);

        if (g_cameraMode == CAMERA_ORBIT)
        {
            if (!io.WantCaptureKeyboard)
            {
                if (g_keys[GLFW_KEY_A]) g_yaw -= 90.0f * dt;
                if (g_keys[GLFW_KEY_D]) g_yaw += 90.0f * dt;
                if (g_keys[GLFW_KEY_W] || g_keys[GLFW_KEY_EQUAL]) g_distance -= g_distance * 1.5f * dt;
                if (g_keys[GLFW_KEY_S] || g_keys[GLFW_KEY_MINUS]) g_distance += g_distance * 1.5f * dt;
            }

            g_distance = std::clamp(g_distance, 0.1f, 500000.0f);

            camPos.x = rotatedCenter.x + g_distance * cos(radPitch) * sin(radYaw);
            camPos.y = rotatedCenter.y + g_distance * sin(radPitch);
            camPos.z = rotatedCenter.z + g_distance * cos(radPitch) * cos(radYaw);

            camFront = glm::normalize(rotatedCenter - camPos);

            bool userActive = g_isMouseDown ||
                              g_keys[GLFW_KEY_A] || g_keys[GLFW_KEY_D] || 
                              g_keys[GLFW_KEY_W] || g_keys[GLFW_KEY_S] ||
                              g_keys[GLFW_KEY_EQUAL] || g_keys[GLFW_KEY_MINUS];

            if (!userActive && (now - g_lastInteractionTime >= 3.0))
            {
                g_yaw += 30.0f * dt;
            }
        }
        else // CAMERA_FPS
        {
            // Przeliczanie wektora kierunku patrzenia
            camFront.x = cos(radPitch) * sin(radYaw);
            camFront.y = sin(radPitch);
            camFront.z = cos(radPitch) * cos(radYaw);
            camFront = glm::normalize(camFront);

            glm::vec3 camRight = glm::normalize(glm::cross(camFront, camUp));

            float speed = g_flySpeed * dt;
            if (g_keys[GLFW_KEY_LEFT_SHIFT]) speed *= 2.5f; // przyspieszenie z Shiftem

            if (!io.WantCaptureKeyboard)
            {
                if (g_keys[GLFW_KEY_W]) g_fpsCameraPos += camFront * speed;
                if (g_keys[GLFW_KEY_S]) g_fpsCameraPos -= camFront * speed;
                if (g_keys[GLFW_KEY_A]) g_fpsCameraPos -= camRight * speed;
                if (g_keys[GLFW_KEY_D]) g_fpsCameraPos += camRight * speed;
                if (g_keys[GLFW_KEY_E] || g_keys[GLFW_KEY_SPACE]) g_fpsCameraPos += camUp * speed;
                if (g_keys[GLFW_KEY_R]) g_fpsCameraPos -= camUp * speed;
            }

            camPos = g_fpsCameraPos;
        }

        g_pitch = std::clamp(g_pitch, -89.0f, 89.0f);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        // --- Panel Lista Plików ---
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(320, (float)height));
        ImGui::Begin("Lista Plikow (.3DS)", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        ImGui::InputText("Szukaj", filterBuffer, sizeof(filterBuffer));
        ImGui::Separator();

        if (ImGui::BeginChild("FileListRegion"))
        {
            std::string filterStr = filterBuffer;
            std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

            for (size_t i = 0; i < g_fileList.size(); ++i)
            {
                std::string filename = fs::path(g_fileList[i]).filename().string();
                std::string filenameLower = filename;
                std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);

                if (!filterStr.empty() && filenameLower.find(filterStr) == std::string::npos)
                {
                    continue;
                }

                bool isSelected = (g_currentFileIdx == i);
                if (ImGui::Selectable(filename.c_str(), isSelected))
                {
                    g_currentFileIdx = i;
                    g_needMeshReload = true;
                    g_lastInteractionTime = glfwGetTime();
                }

                // Przewijaj TYLKO wtedy, gdy explicite ustawiono flagę w keyCallback
                if (isSelected && g_scrollToSelected)
                {
                    ImGui::SetScrollHereY();
                    g_scrollToSelected = false; // Reset po wykonaniu skoku
                }
            }
        }
        ImGui::EndChild();
        ImGui::End();

        // --- Panel HUD ---
        if (g_showHUD)
        {
            ImGui::SetNextWindowPos(ImVec2((float)width - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.65f);

            ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration | 
                                       ImGuiWindowFlags_AlwaysAutoResize | 
                                       ImGuiWindowFlags_NoSavedSettings | 
                                       ImGuiWindowFlags_NoFocusOnAppearing | 
                                       ImGuiWindowFlags_NoNav | 
                                       ImGuiWindowFlags_NoMove;

            if (ImGui::Begin("StatusHUD", nullptr, hudFlags))
            {
                std::string filename = fs::path(g_fileList[g_currentFileIdx]).filename().string();
                
                ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.0f, 1.0f), "[%d/%d] %s", 
                                   (int)(g_currentFileIdx + 1), (int)g_fileList.size(), filename.c_str());
                ImGui::Separator();

                // Wybór trybu kamery
                const char* camModeNames[] = { "Obiekt / Orbit (C)", "Latanie FPS / Swiat (C)" };
                int currentCamInt = static_cast<int>(g_cameraMode);
                if (ImGui::Combo("Kamera (C)", &currentCamInt, camModeNames, IM_ARRAYSIZE(camModeNames)))
                {
                    switchCameraMode(static_cast<CameraMode>(currentCamInt), rotatedCenter);
                }

                if (g_cameraMode == CAMERA_FPS)
                {
                    ImGui::SliderFloat("Predkosc (Rolka)", &g_flySpeed, 10.0f, 10000.0f, "%.0f");
                }

                ImGui::Separator();

                const char* modeNames[] = { "Tekstura (Textured)", "Jednolity kolor (Solid)", "Siatka (Wireframe)" };
                int currentModeInt = static_cast<int>(g_renderMode);
                if (ImGui::Combo("Tryb widoku (M)", &currentModeInt, modeNames, IM_ARRAYSIZE(modeNames)))
                {
                    g_renderMode = static_cast<RenderMode>(currentModeInt);
                }

                ImGui::Checkbox("Przezroczystosc (Alpha) (O)", &g_enableAlphaTest);
                if (!g_enableAlphaTest)
                {
                    ImGui::SameLine();
                    ImGui::Checkbox("Pokaz strukture placht", &g_showAlphaBounds);
                }
                ImGui::Checkbox("Oswietlenie Scene (L)", &g_enableLighting);
                ImGui::Checkbox("Pokaz HUD (T)", &g_showHUD);
                ImGui::Separator();
                ImGui::Text("ESC / Q: Wyjscie");
                ImGui::Text("Gora / Dol / N / P: Zmiana pliku");

                ImGui::Text("C: Zmiana trybu kamery");
                if (g_cameraMode == CAMERA_ORBIT)
                {
                    ImGui::Text("LPM / A / D: Obrot | Rolka/ W /S: Zoom");
                }
                else
                {
                    ImGui::Text("LPM: Rozgladanie sie");
                    ImGui::Text("WASD: Przod/Lewo/Tyl/Prawo");
                    ImGui::Text("E / R: Gora / Dol");
                    ImGui::Text("Shift: Szybki ruch");
                }
            }
            ImGui::End();
        }

        // --- OKIENKO REGULACJI ŚWIATŁA ---
        if (g_enableLighting)
        {
            ImGui::SetNextWindowPos(ImVec2((float)width - 10.0f, 210.0f), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);

            ImGui::Begin("Ustawienia Swiatla", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::SliderFloat("Kat obrotu", &g_lightAngle, 0.0f, 360.0f, "%.1f deg");
            ImGui::SliderFloat("Moc swiatla", &g_lightIntensity, 0.0f, 5.0f, "%.2f");
            ImGui::SliderFloat("Jasnosc cieni (Ambient)", &g_ambientIntensity, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Odleglosc", &g_lightDistanceMult, 0.1f, 3.0f, "%.2f x");
            ImGui::SliderFloat("Wysokosc", &g_lightHeightOffset, 0.0f, 2.0f, "%.2f x");

            ImGui::Separator();
            if (ImGui::Button("Szybkie podswietlenie cieni"))
            {
                g_ambientIntensity = 0.8f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Resetuj swiatlo"))
            {
                g_lightAngle = 45.0f;
                g_lightIntensity = 1.4f;
                g_ambientIntensity = 0.25f;
                g_lightDistanceMult = 0.75f;
                g_lightHeightOffset = 0.5f;
            }

            ImGui::End();
        }

        // Renderowanie sceny 3D
        glViewport(0, 0, width, height);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        if (g_renderMode == RENDER_WIREFRAME)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
        else
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);
        
        float nearPlane = std::max(0.1f, mesh.maxDimension * 0.01f);
        float farPlane  = std::max(1000.0f, mesh.maxDimension * 100.0f);
        glm::mat4 proj  = glm::perspective(glm::radians(45.0f), (float)width / (float)height, nearPlane, farPlane);
        
        glm::mat4 MVP   = proj * view * model;

        // OBLICZANIE POZYCJI ŚWIATŁA NA PODSTAWIE SUWAKÓW:
        float lightRad = glm::radians(g_lightAngle);
        float lightRadius = mesh.maxDimension * g_lightDistanceMult;

        glm::vec3 lightPos = glm::vec3(
            mesh.center.x + lightRadius * sin(lightRad),
            mesh.maxBounds.y + (mesh.maxDimension * g_lightHeightOffset),
            mesh.center.z + lightRadius * cos(lightRad)
        );

        // 1. Rysowanie Modelu 3DS
        glUseProgram(meshProgram);
        glUniformMatrix4fv(glGetUniformLocation(meshProgram, "MVP"), 1, GL_FALSE, glm::value_ptr(MVP));
        glUniformMatrix4fv(glGetUniformLocation(meshProgram, "Model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(glGetUniformLocation(meshProgram, "uLightPos"), 1, glm::value_ptr(lightPos));
        glUniform1f(glGetUniformLocation(meshProgram, "uLightIntensity"), g_lightIntensity);
        glUniform1f(glGetUniformLocation(meshProgram, "uAmbientIntensity"), g_ambientIntensity);
        glUniform1i(glGetUniformLocation(meshProgram, "uEnableLighting"), g_enableLighting);
        glUniform1i(glGetUniformLocation(meshProgram, "uRenderMode"), static_cast<int>(g_renderMode));
        glUniform1i(glGetUniformLocation(meshProgram, "uEnableAlphaTest"), g_enableAlphaTest);
        glUniform1i(glGetUniformLocation(meshProgram, "uShowAlphaBounds"), g_showAlphaBounds);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, activeTex.id);
        glUniform1i(glGetUniformLocation(meshProgram, "uTexture"), 0);

        // Włączenie obsługi kanału Alpha
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (!mesh.faces.empty())
        {
            glBindVertexArray(meshVAO);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.faces.size() * 3), GL_UNSIGNED_SHORT, 0);
        }

        // Wyłączenie blendingu dla kolejnych obiektów
        glDisable(GL_BLEND);

        // 2. Rysowanie Kostki Źródła Światła
        if (g_enableLighting)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            
            glUseProgram(lightProgram);

            // Wskaźnik światła (żółta kostka)
            float lightCubeSize = std::max(0.2f, mesh.maxDimension * 0.05f);
            glm::mat4 lightModel = glm::translate(glm::mat4(1.0f), lightPos);
            lightModel = glm::scale(lightModel, glm::vec3(lightCubeSize));
            glm::mat4 lightMVP = proj * view * lightModel;

            glUniformMatrix4fv(glGetUniformLocation(lightProgram, "MVP"), 1, GL_FALSE, glm::value_ptr(lightMVP));
            glUniform3f(glGetUniformLocation(lightProgram, "uColor"), 1.0f, 0.95f, 0.3f);

            glBindVertexArray(lightVAO);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    if (activeTex.id != fallbackTex.id) activeTex.free();
    fallbackTex.free();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &meshVAO);
    glDeleteBuffers(1, &meshVBO);
    glDeleteBuffers(1, &meshEBO);

    glDeleteVertexArrays(1, &lightVAO);
    glDeleteBuffers(1, &lightVBO);
    glDeleteBuffers(1, &lightEBO);

    glDeleteProgram(meshProgram);
    glDeleteProgram(lightProgram);

    glfwTerminate();
    return 0;
}