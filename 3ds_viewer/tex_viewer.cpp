#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#include "texture_loader.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cstdlib>

namespace fs = std::filesystem;

static std::vector<std::string> g_texFileList;
static size_t g_currentTexIdx = 0;
static Texture2D g_currentTexture;
static bool g_needReload = false;

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;

    if (action == GLFW_PRESS) {
        // Wyjście z aplikacji (ESC lub Q)
        if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // Nawigacja po plikach (Strzałki Górą/Dół, N/P)
        if (!g_texFileList.empty()) {
            if (key == GLFW_KEY_DOWN || key == GLFW_KEY_N) {
                g_currentTexIdx = (g_currentTexIdx + 1) % g_texFileList.size();
                g_needReload = true;
            }
            if (key == GLFW_KEY_UP || key == GLFW_KEY_P) {
                g_currentTexIdx = (g_currentTexIdx + g_texFileList.size() - 1) % g_texFileList.size();
                g_needReload = true;
            }
        }
    }
}

void scanTextureFiles(const std::string& baseDir) {
    fs::path targetPath = fs::path(baseDir) / "_Work" / "Data" / "Textures";
    if (!fs::exists(targetPath)) {
        targetPath = baseDir;
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(targetPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (ext == ".tga" || ext == ".png" || ext == ".jpg" || ext == ".bmp" || ext == ".tex") {
                    g_texFileList.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Blad skanowania tekstur: " << e.what() << std::endl;
    }

    std::sort(g_texFileList.begin(), g_texFileList.end());
}

int main(int argc, char** argv) {
    if (argc > 1) {
        g_texFileList.push_back(argv[1]);
    } else {
        const char* envPath = std::getenv("GOTHIC2_DIR");
        if (envPath) scanTextureFiles(envPath);
    }

    if (!glfwInit()) return 1;

    GLFWwindow* window = glfwCreateWindow(1100, 700, "Gothic Texture Viewer", nullptr, nullptr);
    if (!window) return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetKeyCallback(window, keyCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    if (!g_texFileList.empty()) {
        g_currentTexture = TextureLoader::loadFromFile(g_texFileList[0]);
    }

    char filterBuffer[128] = "";
    float zoomLevel = 1.0f;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (g_needReload && !g_texFileList.empty()) {
            g_currentTexture.free();
            g_currentTexture = TextureLoader::loadFromFile(g_texFileList[g_currentTexIdx]);
            g_needReload = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        // --- PANEL LEWY: Lista Plików ---
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(350, (float)height));
        ImGui::Begin("Tekstury Gothic", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        ImGui::Text("Znaleziono: %zu plikow", g_texFileList.size());
        ImGui::InputText("Szukaj", filterBuffer, sizeof(filterBuffer));
        ImGui::Separator();

        if (ImGui::BeginChild("TexListRegion")) {
            std::string filterStr = filterBuffer;
            std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

            for (size_t i = 0; i < g_texFileList.size(); ++i) {
                std::string filename = fs::path(g_texFileList[i]).filename().string();
                std::string filenameLower = filename;
                std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);

                if (!filterStr.empty() && filenameLower.find(filterStr) == std::string::npos) {
                    continue;
                }

                bool isSelected = (g_currentTexIdx == i);
                if (ImGui::Selectable(filename.c_str(), isSelected)) {
                    g_currentTexIdx = i;
                    g_needReload = true;
                }

                if (isSelected && ImGui::IsWindowFocused()) {
                    ImGui::SetScrollHereY();
                }
            }
        }
        ImGui::EndChild();
        ImGui::End();

        // --- PANEL PRAWY: Podgląd Tekstury ---
        ImGui::SetNextWindowPos(ImVec2(350, 0));
        ImGui::SetNextWindowSize(ImVec2((float)width - 350, (float)height));
        ImGui::Begin("Podglad Tekstury", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        if (g_currentTexture.valid) {
            std::string filename = fs::path(g_currentTexture.path).filename().string();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[%d/%d] %s", (int)(g_currentTexIdx + 1), (int)g_texFileList.size(), filename.c_str());
            ImGui::Text("Wymiary: %d x %d px | Kanały: %d", g_currentTexture.width, g_currentTexture.height, g_currentTexture.channels);
            
            ImGui::SliderFloat("Zoom", &zoomLevel, 0.1f, 5.0f);
            ImGui::SameLine();
            if (ImGui::Button("Reset")) zoomLevel = 1.0f;

            ImGui::Separator();

            ImVec2 displaySize((float)g_currentTexture.width * zoomLevel, (float)g_currentTexture.height * zoomLevel);

            if (ImGui::BeginChild("ImageRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar)) {
                ImGui::Image((void*)(intptr_t)g_currentTexture.id, displaySize);
            }
            ImGui::EndChild();
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Nie udalo sie wczytac wybranego pliku.");
            if (!g_texFileList.empty()) {
                ImGui::TextWrapped("Ścieżka: %s", g_texFileList[g_currentTexIdx].c_str());
            }
        }

        ImGui::End();

        glViewport(0, 0, width, height);
        glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    g_currentTexture.free();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}