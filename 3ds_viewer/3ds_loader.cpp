#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <cstdlib>

#include "stb_easy_font.h"

namespace fs = std::filesystem;

// ---------------------------------------------------------------------
// 1. Parser plików .3DS
// ---------------------------------------------------------------------
struct Vertex {
    glm::vec3 pos;
};

struct Face {
    uint16_t a, b, c;
};

struct Mesh3DS {
    std::vector<Vertex> vertices;
    std::vector<Face> faces;
};

class Loader3DS {
public:
    static bool load(const std::string& filepath, Mesh3DS& outMesh) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) return false;

        outMesh.vertices.clear();
        outMesh.faces.clear();

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
                    file.read(reinterpret_cast<char*>(mesh.vertices.data()), numVertices * sizeof(Vertex));
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
                default:
                    file.seekg(nextChunk, std::ios::beg);
                    break;
            }
        }
    }
};

// ---------------------------------------------------------------------
// 2. Globale i Sterowanie
// ---------------------------------------------------------------------
static float g_yaw = 0.0f;
static float g_pitch = 15.0f;
static float g_distance = 200.0f;
static bool g_keys[512] = {};
static double g_lastInteractionTime = 0.0;

// Stan myszy
static bool g_isMouseDown = false;
static double g_lastMouseX = 0.0;
static double g_lastMouseY = 0.0;

static std::vector<std::string> g_fileList;
static size_t g_currentFileIdx = 0;
static bool g_needMeshReload = false;

// Obsługa przycisków myszy
static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
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

// Obsługa ruchu myszy
static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    if (g_isMouseDown) {
        float dx = static_cast<float>(xpos - g_lastMouseX);
        float dy = static_cast<float>(ypos - g_lastMouseY);

        float sensitivity = 0.3f;
        g_yaw += dx * sensitivity;
        g_pitch -= dy * sensitivity; // Odwrócona oś Y dla intuicyjnego obrotu

        g_lastMouseX = xpos;
        g_lastMouseY = ypos;
        g_lastInteractionTime = glfwGetTime();
    }
}

// Obsługa rolki myszy (zoom)
static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    g_distance -= static_cast<float>(yoffset) * (g_distance * 0.1f);
    g_distance = std::clamp(g_distance, 1.0f, 50000.0f);
    g_lastInteractionTime = glfwGetTime();
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
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
        if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
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

    std::cout << "Przeszukiwanie katalogu: " << targetPath << std::endl;

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
        std::cerr << "Blad podczas skanowania katalogu: " << e.what() << std::endl;
    }

    std::sort(g_fileList.begin(), g_fileList.end());
    std::cout << "Znaleziono plikow .3DS: " << g_fileList.size() << std::endl;
}

// ---------------------------------------------------------------------
// 3. Shadery (Model + Tekst 2D)
// ---------------------------------------------------------------------
static const char* MESH_VERT_SRC = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 MVP;

void main() {
    gl_Position = MVP * vec4(aPos, 1.0);
}
)GLSL";

static const char* MESH_FRAG_SRC = R"GLSL(
#version 330 core
out vec4 FragColor;

void main() {
    FragColor = vec4(0.0, 0.8, 1.0, 1.0);
}
)GLSL";

static const char* TEXT_VERT_SRC = R"GLSL(
#version 330 core
layout (location = 0) in vec2 aPos;
uniform mat4 Proj;

void main() {
    gl_Position = Proj * vec4(aPos, 0.0, 1.0);
}
)GLSL";

static const char* TEXT_FRAG_SRC = R"GLSL(
#version 330 core
out vec4 FragColor;

void main() {
    FragColor = vec4(1.0, 0.9, 0.0, 1.0);
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

void renderText(GLuint textVAO, GLuint textVBO, GLuint textProgram, const std::string& text, float x, float y, float scale, int screenW, int screenH) {
    static char buffer[99999];
    int numQuads = stb_easy_font_print(0, 0, const_cast<char*>(text.c_str()), nullptr, buffer, sizeof(buffer));

    struct StbVertex { float x, y, z; unsigned char color[4]; };
    StbVertex* verts = reinterpret_cast<StbVertex*>(buffer);

    std::vector<glm::vec2> triPoints;
    triPoints.reserve(numQuads * 6);

    for (int i = 0; i < numQuads; ++i) {
        StbVertex v0 = verts[i * 4 + 0];
        StbVertex v1 = verts[i * 4 + 1];
        StbVertex v2 = verts[i * 4 + 2];
        StbVertex v3 = verts[i * 4 + 3];

        triPoints.push_back(glm::vec2(x + v0.x * scale, y + v0.y * scale));
        triPoints.push_back(glm::vec2(x + v1.x * scale, y + v1.y * scale));
        triPoints.push_back(glm::vec2(x + v2.x * scale, y + v2.y * scale));

        triPoints.push_back(glm::vec2(x + v0.x * scale, y + v0.y * scale));
        triPoints.push_back(glm::vec2(x + v2.x * scale, y + v2.y * scale));
        triPoints.push_back(glm::vec2(x + v3.x * scale, y + v3.y * scale));
    }

    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, triPoints.size() * sizeof(glm::vec2), triPoints.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glEnableVertexAttribArray(0);

    glUseProgram(textProgram);

    glm::mat4 proj = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f);
    glUniformMatrix4fv(glGetUniformLocation(textProgram, "Proj"), 1, GL_FALSE, glm::value_ptr(proj));

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(triPoints.size()));
}

// ---------------------------------------------------------------------
// 4. Główny program
// ---------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc > 1) {
        g_fileList.push_back(argv[1]);
    } else {
        const char* envPath = std::getenv("GOTHIC2_DIR");
        if (envPath) {
            scanGothicDirectory(envPath);
        }
    }

    if (g_fileList.empty()) {
        std::cerr << "Brak pliku .3ds! Podaj go jako argument lub ustaw zmienna GOTHIC2_DIR." << std::endl;
        return -1;
    }

    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Gothic 3DS Viewer", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);

    GLuint meshVAO, meshVBO, meshEBO;
    glGenVertexArrays(1, &meshVAO);
    glGenBuffers(1, &meshVBO);
    glGenBuffers(1, &meshEBO);

    GLuint textVAO, textVBO;
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);

    GLuint meshProgram = createProgram(MESH_VERT_SRC, MESH_FRAG_SRC);
    GLuint textProgram = createProgram(TEXT_VERT_SRC, TEXT_FRAG_SRC);

    Mesh3DS mesh;
    auto uploadMesh = [&](const std::string& path) {
        std::cout << "\n[" << (g_currentFileIdx + 1) << "/" << g_fileList.size() << "] Laduje model: " << path << std::endl;

        if (Loader3DS::load(path, mesh)) {
            glBindVertexArray(meshVAO);

            glBindBuffer(GL_ARRAY_BUFFER, meshVBO);
            glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(Vertex), mesh.vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.faces.size() * sizeof(Face), mesh.faces.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
            glEnableVertexAttribArray(0);

            std::cout << "-> Wczytano wierzcholkow: " << mesh.vertices.size() << ", scian: " << mesh.faces.size() << std::endl;
        } else {
            std::cerr << "-> Blad wczytywania pliku!" << std::endl;
        }
    };

    uploadMesh(g_fileList[g_currentFileIdx]);
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        glfwPollEvents();

        if (g_needMeshReload) {
            uploadMesh(g_fileList[g_currentFileIdx]);
            g_needMeshReload = false;
        }

        // Klawisze obrotu
        if (g_keys[GLFW_KEY_LEFT])  g_yaw -= 90.0f * dt;
        if (g_keys[GLFW_KEY_RIGHT]) g_yaw += 90.0f * dt;
        if (g_keys[GLFW_KEY_UP])    g_pitch += 90.0f * dt;
        if (g_keys[GLFW_KEY_DOWN])  g_pitch -= 90.0f * dt;

        // Klawisze zoomu (W/S lub +/-)
        if (g_keys[GLFW_KEY_W] || g_keys[GLFW_KEY_EQUAL]) g_distance -= g_distance * 1.5f * dt;
        if (g_keys[GLFW_KEY_S] || g_keys[GLFW_KEY_MINUS]) g_distance += g_distance * 1.5f * dt;

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

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);

        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- 1. RENDEROWANIE MODELU 3D ---
        glEnable(GL_DEPTH_TEST);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
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

        if (!mesh.faces.empty()) {
            glBindVertexArray(meshVAO);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.faces.size() * 3), GL_UNSIGNED_SHORT, 0);
        }

        // --- 2. RENDEROWANIE TEKSTU 2D ---
        glDisable(GL_DEPTH_TEST);

        std::string filename = fs::path(g_fileList[g_currentFileIdx]).filename().string();
        std::string infoText = "[" + std::to_string(g_currentFileIdx + 1) + "/" + std::to_string(g_fileList.size()) + "] " + filename;
        std::string controlsText = "N/P: Zmiana pliku | LPM + Przeciagnij / Strzalki: Kamera | Zoom: Rolka/W/S";

        renderText(textVAO, textVBO, textProgram, infoText, 20.0f, 20.0f, 2.0f, width, height);
        renderText(textVAO, textVBO, textProgram, controlsText, 20.0f, 50.0f, 1.5f, width, height);

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &meshVAO);
    glDeleteBuffers(1, &meshVBO);
    glDeleteBuffers(1, &meshEBO);
    glDeleteVertexArrays(1, &textVAO);
    glDeleteBuffers(1, &textVBO);
    glDeleteProgram(meshProgram);
    glDeleteProgram(textProgram);

    glfwTerminate();
    return 0;
}