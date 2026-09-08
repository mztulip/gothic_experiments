#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <map>
#include <memory>

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "vfs_loader.hpp"

namespace fs = std::filesystem;

struct ConsoleTreeNode 
{
    std::string name;
    bool is_directory = false;
    std::map<std::string, std::shared_ptr<ConsoleTreeNode>> children;
};

void build_console_tree(const zenkit::VfsNode& node, std::shared_ptr<ConsoleTreeNode>& current_console_node) 
{
    if (node.type() == zenkit::VfsNodeType::DIRECTORY) 
    {
        for (const auto& child : node.children()) 
        {
            auto child_console_node = std::make_shared<ConsoleTreeNode>();
            child_console_node->name = child.name();
            child_console_node->is_directory = (child.type() == zenkit::VfsNodeType::DIRECTORY);

            current_console_node->children[child.name()] = child_console_node;

            if (child.type() == zenkit::VfsNodeType::DIRECTORY) 
            {
                build_console_tree(child, child_console_node);
            }
        }
    }
}

void print_console_tree_recursive(const std::shared_ptr<ConsoleTreeNode>& node, const std::string& indent = "", bool is_last = true) 
{
    if (!node->name.empty()) 
    {
        std::cout << indent;
        std::cout << (is_last ? "└── " : "├── ");
        std::cout << node->name << "\n";
    }

    std::string child_indent = indent;
    if (!node->name.empty()) 
    {
        child_indent += (is_last ? "    " : "│   ");
    }

    size_t total_children = node->children.size();
    size_t current_index = 0;

    for (const auto& [name, child] : node->children) 
    {
        bool last_child = (++current_index == total_children);
        print_console_tree_recursive(child, child_indent, last_child);
    }
}

void print_vfs_tree_console(const zenkit::VfsNode& root_node) 
{
    auto root = std::make_shared<ConsoleTreeNode>();
    root->name = "";
    root->is_directory = true;

    build_console_tree(root_node, root);

    std::cout << "\n================ VFS TREE DUMP ================\n/\n";
    print_console_tree_recursive(root);
    std::cout << "===============================================\n";
}

// Funkcja rekurencyjna do wyciągania całej struktury VFS na dysk
void extract_node_recursive(const zenkit::VfsNode& node, const fs::path& current_dest_path) 
{
    if (node.type() == zenkit::VfsNodeType::DIRECTORY) 
    {
        fs::path dir_path = current_dest_path / node.name();
        fs::create_directories(dir_path);

        for (const auto& child : node.children()) 
        {
            extract_node_recursive(child, dir_path);
        }
    } 
    else 
    {
        fs::path file_path = current_dest_path / node.name();
        try 
        {
            auto rd = node.open_read();
            std::ofstream out(file_path, std::ios::binary);

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
            std::cout << "Wypakowano: " << file_path.string() << "\n";
        } 
        catch (const std::exception& e) 
        {
            std::cerr << "Blad podczas wypakowywania " << file_path.string() << ": " << e.what() << "\n";
        }
    }
}

void extract_vfs_to_directory(const zenkit::VfsNode& root_node, const std::string& dest_dir) 
{
    fs::path target_dir(dest_dir);
    
    std::cout << "\nRozpoczynanie wypakowywania VFS do: " << fs::absolute(target_dir).string() << "\n";
    
    for (const auto& child : root_node.children()) 
    {
        extract_node_recursive(child, target_dir);
    }
    
    std::cout << "Wypakowywanie zakonczone sukcesem!\n";
}

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

        ImGui::Text("Sciezka:");
        ImGui::SameLine();
        ImGui::InputText("##gothicpath", path_input_buf, sizeof(path_input_buf));
        ImGui::SameLine();
        if (ImGui::Button("Zaladuj Data/")) 
        {
            load_gothic_dir(path_input_buf);
        }

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
    std::string target_path;
    std::string extract_dest_path;
    bool dump_tree_only = false;

    // Parsowanie argumentow CLI
    for (int i = 1; i < argc; ++i) 
    {
        std::string arg = argv[i];
        if (arg == "-tree" || arg == "--tree") 
        {
            dump_tree_only = true;
        } 
        else if ((arg == "-extract" || arg == "-e" || arg == "--extract") && i + 1 < argc) 
        {
            extract_dest_path = argv[++i];
        } 
        else if (target_path.empty()) 
        {
            target_path = arg;
        }
    }

    // Fallback na zmienna srodowiskowa GOTHIC2_DIR
    if (target_path.empty()) 
    {
        if (const char* env_dir = std::getenv("GOTHIC2_DIR")) 
        {
            target_path = env_dir;
        }
    }

    // Tryb konsolowy - zrzut drzewa
    if (dump_tree_only) 
    {
        if (target_path.empty()) 
        {
            std::cerr << "Blad: Nie podano sciezki do Gothica ani nie ustawiono GOTHIC2_DIR.\n";
            return 1;
        }

        zenkit::Vfs& vfs = gothicVfs(target_path);
        print_vfs_tree_console(vfs.root());
        return 0;
    }

    // Tryb konsolowy - wypakowanie calej struktury
    if (!extract_dest_path.empty()) 
    {
        if (target_path.empty()) 
        {
            std::cerr << "Blad: Nie podano sciezki do Gothica ani nie ustawiono GOTHIC2_DIR.\n";
            return 1;
        }

        zenkit::Vfs& vfs = gothicVfs(target_path);
        extract_vfs_to_directory(vfs.root(), extract_dest_path);
        return 0;
    }

    // Tryb GUI z GLFW/ImGui
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

    if (!target_path.empty()) 
    {
        snprintf(app.path_input_buf, sizeof(app.path_input_buf), "%s", target_path.c_str());
        app.load_gothic_dir(target_path);
    }

    while (!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();

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