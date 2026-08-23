#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

// ---------------------------------------------------------------------
// 1. Parser plików .3DS z obsługą koordynatów UV i Nazw Materiałów
// ---------------------------------------------------------------------
struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;
};

struct Face {
    uint16_t a, b, c;
};

struct Mesh3DS {
    std::vector<Vertex> vertices;
    std::vector<Face> faces;
    std::string textureFile;
};

class Loader3DS {
public:
    static bool load(const std::string& filepath, Mesh3DS& outMesh) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) return false;

        outMesh.vertices.clear();
        outMesh.faces.clear();
        outMesh.textureFile = "";

        file.seekg(0, std::ios::end);
        uint32_t fileSize = static_cast<uint32_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        parseChunk(file, fileSize, outMesh);
        return !outMesh.vertices.empty();
    }

private:
    static void parseChunk(std::ifstream& file, uint32_t endPos, Mesh3DS& mesh) {
        while (file.tellg() < endPos && file.good()) {
            uint16_t chunkId;
            uint32_t chunkLength;

            file.read(reinterpret_cast<char*>(&chunkId), sizeof(chunkId));
            file.read(reinterpret_cast<char*>(&chunkLength), sizeof(chunkLength));

            uint32_t nextChunk = static_cast<uint32_t>(file.tellg()) + chunkLength - 6;

            switch (chunkId) {
                case 0x4D4D: // MAIN3DS
                case 0x3D3D: // EDIT3DS
                case 0x4000: // TRI_OBJECT
                case 0xAFFF: // MAT_ENTRY
                case 0xA200: // MAT_TEXMAP
                {
                    if (chunkId == 0x4000) {
                        char ch;
                        while (file.get(ch) && ch != '\0');
                    }
                    parseChunk(file, nextChunk, mesh);
                    break;
                }
                case 0x4100: // N_TRI_OBJECT
                    parseChunk(file, nextChunk, mesh);
                    break;

                case 0x4110: // POINT_ARRAY
                {
                    uint16_t numVertices;
                    file.read(reinterpret_cast<char*>(&numVertices), sizeof(numVertices));
                    mesh.vertices.resize(numVertices);
                    for (int i = 0; i < numVertices; ++i) {
                        file.read(reinterpret_cast<char*>(&mesh.vertices[i].pos), sizeof(glm::vec3));
                        mesh.vertices[i].uv = glm::vec2(0.0f, 0.0f);
                    }
                    break;
                }
                case 0x4140: // TEX_VERTS (UV Coordinates)
                {
                    uint16_t numUVs;
                    file.read(reinterpret_cast<char*>(&numUVs), sizeof(numUVs));
                    if (numUVs == mesh.vertices.size()) {
                        for (int i = 0; i < numUVs; ++i) {
                            file.read(reinterpret_cast<char*>(&mesh.vertices[i].uv), sizeof(glm::vec2));
                        }
                    } else {
                        file.seekg(nextChunk, std::ios::beg);
                    }
                    break;
                }
                case 0x4120: // FACE_ARRAY
                {
                    uint16_t numFaces;
                    file.read(reinterpret_cast<char*>(&numFaces), sizeof(numFaces));
                    mesh.faces.resize(numFaces);
                    for (int i = 0; i < numFaces; ++i) {
                        file.read(reinterpret_cast<char*>(&mesh.faces[i]), 3 * sizeof(uint16_t));
                        uint16_t flags;
                        file.read(reinterpret_cast<char*>(&flags), sizeof(flags));
                    }
                    break;
                }
                case 0xA300: // MAT_MAPNAME
                {
                    char ch;
                    std::string texName = "";
                    while (file.get(ch) && ch != '\0') {
                        texName += ch;
                    }
                    mesh.textureFile = texName;
                    file.seekg(nextChunk, std::ios::beg);
                    break;
                }
                default:
                    file.seekg(nextChunk, std::ios::beg);
                    break;
            }
        }
    }
};

// ---------------------------------------------------------------------
// 2. Globale i Zmienne Stanu
// ---------------------------------------------------------------------
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

// Opcje Widoków:
static bool g_wireframeMode = true;   // Klawisz M: Przełącza Siatkę / Teksturę
static bool g_showHUD = true;         // Klawisz T: Ukrywa/Pokazuje Tekst i HUD

// Callbacks GLFW
static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            g_isMouseDown = true;
            glfwGetCursorPos(window, &g_lastMouseX, &g_lastMouseY);
            g_lastInteractionTime = glfwGetTime();
        } else if (action == GLFW_RELEASE) {
            g_isMouseDown = false;
            g_lastInteractionTime = glfwGetTime();
        }
    }
}

static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    if (g_isMouseDown) {
        float dx = static_cast<float>(xpos - g_lastMouseX);
        float dy = static_cast<float>(ypos - g_lastMouseY);

        float sensitivity = 0.3f;
        g_yaw += dx * sensitivity;
        g_pitch -= dy * sensitivity;

        g_lastMouseX = xpos;
        g_lastMouseY = ypos;
        g_lastInteractionTime = glfwGetTime();
    }
}

static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    g_distance -= static_cast<float>(yoffset) * (g_distance * 0.1f);
    g_distance = std::clamp(g_distance, 1.0f, 50000.0f);
    g_lastInteractionTime = glfwGetTime();
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;

    if (key >= 0 && key < 512) {
        if (action == GLFW_PRESS)   g_keys[key] = true;
        if (action == GLFW_RELEASE) g_keys[key] = false;
    }

    if (key == GLFW_KEY_LEFT || key == GLFW_KEY_RIGHT || 
        key == GLFW_KEY_UP   || key == GLFW_KEY_DOWN  ||
        key == GLFW_KEY_EQUAL || key == GLFW_KEY_MINUS ||
        key == GLFW_KEY_W || key == GLFW_KEY_S) {
        g_lastInteractionTime = glfwGetTime();
    }

    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // Klawisz M: Przełączanie trybu Siatka / Tekstury
        if (key == GLFW_KEY_M) {
            g_wireframeMode = !g_wireframeMode;
        }

        // Klawisz T: Pokazuj/Ukrywaj tekst i HUD
        if (key == GLFW_KEY_T) {
            g_showHUD = !g_showHUD;
        }

        if (!g_fileList.empty()) {
            if (key == GLFW_KEY_N) {
                g_currentFileIdx = (g_currentFileIdx + 1) % g_fileList.size();
                g_needMeshReload = true;
            }
            if (key == GLFW_KEY_P) {
                g_currentFileIdx = (g_currentFileIdx + g_fileList.size() - 1) % g_fileList.size();
                g_needMeshReload = true;
            }
        }
    }
}

void scanGothicDirectory(const std::string& gothicDir) {
    fs::path targetPath = fs::path(gothicDir) / "_Work" / "Data" / "Meshes";
    if (!fs::exists(targetPath)) {
        targetPath = gothicDir;
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(targetPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".3ds") {
                    g_fileList.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Blad skanowania: " << e.what() << std::endl;
    }

    std::sort(g_fileList.begin(), g_fileList.end());
}

// Generowanie awaryjnej tekstury (Szachownica)
GLuint createFallbackTexture() {
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    uint32_t pixels[16 * 16];
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            bool check = ((x / 2) + (y / 2)) % 2 == 0;
            pixels[y * 16 + x] = check ? 0xFFFFFFFF : 0xFF808080;
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texID;
}

// ---------------------------------------------------------------------
// 3. Shadery Modelu (Z obsługą tekstur i koloru)
// ---------------------------------------------------------------------
static const char* MESH_VERT_SRC = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
uniform mat4 MVP;

void main() {
    gl_Position = MVP * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)GLSL";

static const char* MESH_FRAG_SRC = R"GLSL(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
uniform sampler2D uTexture;
uniform bool uUseTexture;

void main() {
    if (uUseTexture) {
        FragColor = texture(uTexture, TexCoord);
    } else {
        FragColor = vec4(0.0, 0.8, 1.0, 1.0);
    }
}
)GLSL";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    return sh;
}

static GLuint createProgram(const char* vSrc, const char* fSrc) {
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

// ---------------------------------------------------------------------
// 4. Główny Program
// ---------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc > 1) {
        g_fileList.push_back(argv[1]);
    } else {
        const char* envPath = std::getenv("GOTHIC2_DIR");
        if (envPath) scanGothicDirectory(envPath);
    }

    if (g_fileList.empty()) {
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

    // Setup Dear ImGui
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

    GLuint meshProgram = createProgram(MESH_VERT_SRC, MESH_FRAG_SRC);
    GLuint fallbackTex = createFallbackTexture();
    GLuint activeTex = fallbackTex;

Mesh3DS mesh;
    auto uploadMesh = [&](const std::string& path) {
        if (Loader3DS::load(path, mesh)) {
            glBindVertexArray(meshVAO);

            glBindBuffer(GL_ARRAY_BUFFER, meshVBO);
            glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(Vertex), mesh.vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.faces.size() * sizeof(Face), mesh.faces.data(), GL_STATIC_DRAW);

            // Pozycje (location 0)
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
            glEnableVertexAttribArray(0);

            // UV (location 1)
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
            glEnableVertexAttribArray(1);

            // Czyszczenie starej tekstury z pamięci GPU (jeśli była customowa)
            if (activeTex != fallbackTex) {
                glDeleteTextures(1, &activeTex);
                activeTex = fallbackTex;
            }

            if (!mesh.textureFile.empty()) {
                std::cout << "[3DS] Model wymaga tekstury: " << mesh.textureFile << std::endl;

                // 1. Szukaj w tym samym folderze co plik .3DS
                fs::path texPath = fs::path(path).parent_path() / mesh.textureFile;

                // 2. Jeśli nie ma, szukaj w globalnym folderze Textures Gothica (jeśli podano GOTHIC2_DIR)
                if (!fs::exists(texPath)) {
                    const char* envPath = std::getenv("GOTHIC2_DIR");
                    if (envPath) {
                        fs::path gothicTexDir = fs::path(envPath) / "_Work" / "Data" / "Textures";
                        if (fs::exists(gothicTexDir)) {
                            try {
                                for (const auto& entry : fs::recursive_directory_iterator(gothicTexDir)) {
                                    if (entry.is_regular_file() && entry.path().filename().string() == mesh.textureFile) {
                                        texPath = entry.path();
                                        break;
                                    }
                                }
                            } catch (...) {}
                        }
                    }
                }

                if (fs::exists(texPath)) {
                    int w, h, comp;
                    stbi_set_flip_vertically_on_load(true);
                    unsigned char* imgData = stbi_load(texPath.string().c_str(), &w, &h, &comp, 4);

                    if (imgData) {
                        GLuint newTex;
                        glGenTextures(1, &newTex);
                        glBindTexture(GL_TEXTURE_2D, newTex);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, imgData);
                        glGenerateMipmap(GL_TEXTURE_2D);
                        stbi_image_free(imgData);
                        activeTex = newTex;
                        std::cout << " -> Zaladowano teksture: " << texPath.string() << std::endl;
                    } else {
                        std::cout << " -> Blad odczytu pliku tekstury przez stb_image!" << std::endl;
                    }
                } else {
                    std::cout << " -> Plik tekstury NIE ISTNIEJE na dysku (uzywam szachownicy)." << std::endl;
                }
            } else {
                std::cout << "[3DS] Model nie posiada przypisanej tekstury w pliku 3DS." << std::endl;
            }
        }
    };
    
    uploadMesh(g_fileList[g_currentFileIdx]);
    double lastTime = glfwGetTime();

    char filterBuffer[128] = "";

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        glfwPollEvents();

        if (g_needMeshReload) {
            uploadMesh(g_fileList[g_currentFileIdx]);
            g_needMeshReload = false;
        }

        // Sterowanie z klawiatury
        if (!io.WantCaptureKeyboard) {
            if (g_keys[GLFW_KEY_LEFT])  g_yaw -= 90.0f * dt;
            if (g_keys[GLFW_KEY_RIGHT]) g_yaw += 90.0f * dt;
            if (g_keys[GLFW_KEY_UP])    g_pitch += 90.0f * dt;
            if (g_keys[GLFW_KEY_DOWN])  g_pitch -= 90.0f * dt;

            if (g_keys[GLFW_KEY_W] || g_keys[GLFW_KEY_EQUAL]) g_distance -= g_distance * 1.5f * dt;
            if (g_keys[GLFW_KEY_S] || g_keys[GLFW_KEY_MINUS]) g_distance += g_distance * 1.5f * dt;
        }

        g_distance = std::clamp(g_distance, 1.0f, 50000.0f);

        bool userActive = g_isMouseDown ||
                          g_keys[GLFW_KEY_LEFT] || g_keys[GLFW_KEY_RIGHT] || 
                          g_keys[GLFW_KEY_UP]   || g_keys[GLFW_KEY_DOWN]  ||
                          g_keys[GLFW_KEY_W]    || g_keys[GLFW_KEY_S]     ||
                          g_keys[GLFW_KEY_EQUAL]|| g_keys[GLFW_KEY_MINUS];

        if (!userActive && (now - g_lastInteractionTime >= 3.0)) {
            g_yaw += 30.0f * dt;
        }

        g_pitch = std::clamp(g_pitch, -89.0f, 89.0f);

        // Start ramki ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        // --- 1. OKNO IMGUI: Lista Plików ---
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(320, (float)height));
        ImGui::Begin("Lista Plikow (.3DS)", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        ImGui::InputText("Szukaj", filterBuffer, sizeof(filterBuffer));
        ImGui::Separator();

        if (ImGui::BeginChild("FileListRegion")) {
            std::string filterStr = filterBuffer;
            std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

            for (size_t i = 0; i < g_fileList.size(); ++i) {
                std::string filename = fs::path(g_fileList[i]).filename().string();
                std::string filenameLower = filename;
                std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);

                if (!filterStr.empty() && filenameLower.find(filterStr) == std::string::npos) {
                    continue;
                }

                bool isSelected = (g_currentFileIdx == i);
                if (ImGui::Selectable(filename.c_str(), isSelected)) {
                    g_currentFileIdx = i;
                    g_needMeshReload = true;
                    g_lastInteractionTime = glfwGetTime();
                }

                if (isSelected && ImGui::IsWindowFocused()) {
                    ImGui::SetScrollHereY();
                }
            }
        }
        ImGui::EndChild();
        ImGui::End();

        // --- 2. OKNO IMGUI: HUD Informacyjny ---
        if (g_showHUD) {
            ImGui::SetNextWindowPos(ImVec2((float)width - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.65f);

            ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration | 
                                       ImGuiWindowFlags_AlwaysAutoResize | 
                                       ImGuiWindowFlags_NoSavedSettings | 
                                       ImGuiWindowFlags_NoFocusOnAppearing | 
                                       ImGuiWindowFlags_NoNav | 
                                       ImGuiWindowFlags_NoMove;

            if (ImGui::Begin("StatusHUD", nullptr, hudFlags)) {
                std::string filename = fs::path(g_fileList[g_currentFileIdx]).filename().string();
                
                ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.0f, 1.0f), "[%d/%d] %s", 
                                   (int)(g_currentFileIdx + 1), (int)g_fileList.size(), filename.c_str());
                ImGui::Separator();
                ImGui::Checkbox("Siatka / Wireframe (M)", &g_wireframeMode);
                ImGui::Checkbox("Pokaz HUD (T)", &g_showHUD);
                ImGui::Text("N/P: Zmiana pliku");
                ImGui::Text("LPM + Przeciagnij / Strzalki: Kamera");
                ImGui::Text("Rolka / W / S / +/-: Zoom (Dysk: %.0f)", g_distance);
            }
            ImGui::End();
        }

        // Renderowanie OpenGL 3D
        glViewport(0, 0, width, height);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        // Przełączanie trybu Siatki vs Tekstury
        if (g_wireframeMode) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        glUseProgram(meshProgram);

        float radYaw = glm::radians(g_yaw);
        float radPitch = glm::radians(g_pitch);

        glm::vec3 camPos;
        camPos.x = g_distance * cos(radPitch) * sin(radYaw);
        camPos.y = g_distance * sin(radPitch);
        camPos.z = g_distance * cos(radPitch) * cos(radYaw);

        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 view  = glm::lookAt(camPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj  = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.5f, 100000.0f);
        glm::mat4 MVP   = proj * view * model;

        glUniformMatrix4fv(glGetUniformLocation(meshProgram, "MVP"), 1, GL_FALSE, glm::value_ptr(MVP));

        // Tekstury w shaderze
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, activeTex);
        glUniform1i(glGetUniformLocation(meshProgram, "uTexture"), 0);
        glUniform1i(glGetUniformLocation(meshProgram, "uUseTexture"), !g_wireframeMode);

        if (!mesh.faces.empty()) {
            glBindVertexArray(meshVAO);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.faces.size() * 3), GL_UNSIGNED_SHORT, 0);
        }

        // Renderowanie GUI na wierzchu
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup ImGui & GL
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &meshVAO);
    glDeleteBuffers(1, &meshVBO);
    glDeleteBuffers(1, &meshEBO);
    glDeleteProgram(meshProgram);

    glfwTerminate();
    return 0;
}