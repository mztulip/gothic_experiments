#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <algorithm>

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "vfs_loader.hpp"

namespace fs = std::filesystem;

struct VdfViewerApp 
{
    zenkit::Vfs* vfs = nullptr;
    char path_input_buf[512] = ""; 
    char search_buf[128] = "";
    std::string status_message = "Podaj sciezke do katalogu glownego Gothica i kliknij Zaladuj.";

    void load_gothic_dir(const std::string& gothic_path) 
    {
        if (gothic_path.empty()) 
        {
            return;
        }
        
        vfs = &gothicVfs(gothic_path);
        status_message = "Zaladowano VFS dla: " + gothic_path;
    }

    void extract_file(const zenkit::VfsNode* node, const std::string& dest_path) 
    {
        if (!vfs || !node)
        {
            return;
        }

        try 
        {
            auto rd = node->open_read();
            std::ofstream out(dest_path, std::ios::binary);

            char buffer[8192];
            while (!rd->eof()) 
            {
                std::size_t bytes_read = rd->read(buffer, sizeof(buffer));
                if (bytes_read == 0)
                {
                    break;
                }
                out.write(buffer, static_cast<std::streamsize>(bytes_read));
            }

            status_message = "Wypakowano: " + dest_path;
        } 
        catch (const std::exception& e) 
        {
            status_message = std::string("Blad wypakowywania: ") + e.what();
        }
    }

    // Pomocnicza funkcja sprawdzajaca czy wezel lub jego dzieci pasuja do filtra
    bool node_matches_filter(const zenkit::VfsNode& node, const std::string& filter) const
    {
        if (filter.empty())
        {
            return true;
        }

        std::string name_lower = node.name();
        std::string filter_lower = filter;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), [](unsigned char c){ return std::tolower(c); });
        std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), [](unsigned char c){ return std::tolower(c); });

        if (name_lower.find(filter_lower) != std::string::npos)
        {
            return true;
        }

        if (node.type() == zenkit::VfsNodeType::DIRECTORY)
        {
            for (const auto& child : node.children())
            {
                if (node_matches_filter(child, filter))
                {
                    return true;
                }
            }
        }

        return false;
    }

    void render_vfs_tree(const zenkit::VfsNode& node, const std::string& filter) 
    {
        if (!node_matches_filter(node, filter))
        {
            return;
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

        // Jesli aktywnie wyszukujemy, rozwijamy drzewo automatycznie
        if (!filter.empty())
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        if (node.type() == zenkit::VfsNodeType::DIRECTORY) 
        {
            if (ImGui::TreeNodeEx(node.name().c_str(), flags)) 
            {
                for (const auto& child : node.children()) 
                {
                    render_vfs_tree(child, filter);
                }
                ImGui::TreePop();
            }
        } 
        else 
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            ImGui::TreeNodeEx(node.name().c_str(), flags);

            if (ImGui::BeginPopupContextItem()) 
            {
                if (ImGui::MenuItem("Wypakuj tutaj")) 
                {
                    extract_file(&node, node.name());
                }
                ImGui::EndPopup();
            }
        }
    }

    void render_ui() 
    {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);

        ImGui::Begin("zEngine VDF Explorer", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        // Sekcja sciezki Gothica
        ImGui::Text("Sciezka:");
        ImGui::SameLine();
        ImGui::InputText("##gothicpath", path_input_buf, sizeof(path_input_buf));
        ImGui::SameLine();
        if (ImGui::Button("Zaladuj Data/")) 
        {
            load_gothic_dir(path_input_buf);
        }

        // Sekcja wyszukiwarki
        ImGui::Text("Szukaj: ");
        ImGui::SameLine();
        ImGui::InputText("##search", search_buf, sizeof(search_buf));

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", status_message.c_str());
        ImGui::Separator();

        if (vfs) 
        {
            ImGui::BeginChild("VfsTreeWindow", ImVec2(0, 0), true);
            render_vfs_tree(vfs->root(), std::string(search_buf));
            ImGui::EndChild();
        } 
        else 
        {
            ImGui::TextDisabled("VFS nie zostal jeszcze zainicjalizowany.");
        }

        ImGui::End();
    }
};

static void glfw_error_callback(int error, const char* description) 
{
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main(int argc, char** argv) 
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) 
    {
        return 1;
    }

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1024, 768, "zEngine VDF Browser", nullptr, nullptr);
    if (!window) 
    {
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    VdfViewerApp app;

    std::string target_path;

    if (argc > 1) 
    {
        target_path = argv[1];
    } 
    else if (const char* env_dir = std::getenv("GOTHIC2_DIR")) 
    {
        target_path = env_dir;
    }

    if (!target_path.empty()) 
    {
        snprintf(app.path_input_buf, sizeof(app.path_input_buf), "%s", target_path.c_str());
        app.load_gothic_dir(target_path);
    }

    while (!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();

        // Zamknięcie programu klawiszem ESC
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app.render_ui();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}