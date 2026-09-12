#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <cmath>
#include <algorithm>

#include <epoxy/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "structs.hpp"
#include "loader.hpp"
#include "camera.hpp"
#include "axis_gizmo.hpp"

const char* vertex_shader_src = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 Normal;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* fragment_shader_src = R"(
#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;

uniform vec3 objectColor;
uniform bool useLighting;
uniform bool opaqueMode;      // NOWY uniform

void main() {
    if (!useLighting) {
        FragColor = vec4(objectColor, 1.0);
        return;
    }

    vec3 lightPos = vec3(100.0, 500.0, 300.0);
    vec3 ambient = 0.3 * objectColor;
    
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * objectColor;

    float alpha = opaqueMode ? 1.0 : 0.6;
    FragColor = vec4(ambient + diffuse, alpha);
}
)";

GLuint compile_shader(GLenum type, const char* src) 
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    return shader;
}

GLuint create_program() 
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);
    
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

struct Vertex {
    glm::vec3 pos;
    glm::vec3 norm;
};

// Pomocnicza funkcja konwersji macierzy z ZenKit do GLM (pamiętając o transpozycji)
glm::mat4 zenkit_to_glm(const zenkit::Mat4& m) {
    return glm::transpose(glm::make_mat4(&m[0][0]));
}

std::vector<RenderMesh> create_body_meshes(
    const zenkit::ModelMesh& model_mesh,
    const std::vector<glm::mat4>& bone_mats)
{
    std::vector<RenderMesh> meshes;

    for (const auto& skin : model_mesh.meshes)
    {
        const auto& mesh = skin.mesh;

        for (const auto& sub : mesh.sub_meshes)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            vertices.reserve(sub.wedges.size());
            indices.reserve(sub.triangles.size() * 3);

            for (size_t i = 0; i < sub.wedges.size(); ++i)
            {
                const auto& wedge = sub.wedges[i];
                uint32_t v_idx = wedge.index;

                if (v_idx >= skin.weights.size()) continue;

                glm::vec3 skinned_pos(0.0f);
                float total_weight = 0.0f;

                for (const auto& w : skin.weights[v_idx])
                {
                    if (w.node_index < bone_mats.size())
                    {
                        // KAŻDA waga ma WŁASNĄ lokalną pozycję względem swojej kości!
                        glm::vec4 local_pos(w.position.x, w.position.y, w.position.z, 1.0f);
                        const glm::mat4& bone_mat = bone_mats[w.node_index];
                        skinned_pos  += w.weight * glm::vec3(bone_mat * local_pos);
                        total_weight += w.weight;
                    }
                }

                // normalna: BEZ transformacji per-kość, dokładnie jak w oryginalnym silniku
                glm::vec3 final_n = glm::vec3(wedge.normal.x, wedge.normal.y, wedge.normal.z);
                glm::vec3 final_p = skinned_pos; // total_weight powinno być ~1.0 (Gothic sam normalizuje wagi)

                glm::vec3 gl_pos  = glm::vec3(final_p.x, final_p.z, -final_p.y);
                glm::vec3 gl_norm = glm::vec3(final_n.x, final_n.z, -final_n.y);

                vertices.push_back({ gl_pos, gl_norm });
            }

            for (const auto& tri : sub.triangles)
            {
                indices.push_back(tri.wedges[0]);
                indices.push_back(tri.wedges[1]);
                indices.push_back(tri.wedges[2]);
            }

            if (indices.empty()) continue;

            RenderMesh rm;
            rm.index_count = static_cast<GLsizei>(indices.size());

            glGenVertexArrays(1, &rm.vao);
            glGenBuffers(1, &rm.vbo);
            glGenBuffers(1, &rm.ebo);

            glBindVertexArray(rm.vao);

            glBindBuffer(GL_ARRAY_BUFFER, rm.vbo);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rm.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_DYNAMIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
            glEnableVertexAttribArray(0);

            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, norm));
            glEnableVertexAttribArray(1);

            glBindVertexArray(0);
            meshes.push_back(rm);
        }
    }

    return meshes;
}

struct BoneAnimResult {
    std::vector<glm::vec3> positions;
    std::vector<glm::mat4> matrices;
};

BoneAnimResult get_animated_bone_positions(
    const zenkit::ModelHierarchy& hierarchy,
    const zenkit::ModelAnimation& anim,
    int frame,
    const std::string& skeleton_name)
{
    size_t num_nodes = hierarchy.nodes.size();
    std::vector<glm::mat4> global(num_nodes, glm::mat4(1.0f));
    std::vector<glm::vec3> out(num_nodes, glm::vec3(0.0f));

    if (num_nodes == 0)
        return BoneAnimResult{ {}, {} };

    uint32_t frame_idx = std::clamp(frame, 0, static_cast<int>(anim.frame_count) - 1);

    for (size_t i = 0; i < num_nodes; ++i)
    {
        const auto& node = hierarchy.nodes[i];
        
        // Domyślna macierz lokalna z hierarchii szkieletu
        glm::mat4 local = zenkit_to_glm(node.transform);

        size_t sidx = frame_idx * num_nodes + i;

        if (sidx < anim.samples.size())
        {
            const auto& s = anim.samples[sidx];

            // W ZenKit kwaterniony próbki animacji tworzymy w kolejności (w, x, y, z)
            glm::quat q(s.rotation.w, s.rotation.x, s.rotation.y, s.rotation.z);
            glm::mat4 R = glm::mat4_cast(q);
            glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(s.position.x, s.position.y, s.position.z));

            // Łączenie rotacji i translacji dla danej klatki
            local = T * R;
        }

        if (node.parent_index != 0xFFFF && node.parent_index < num_nodes)
            global[i] = global[node.parent_index] * local;
        else
            global[i] = local;

        glm::vec4 p = global[i][3];
        // Pozycja kości do rysowania szkieletu w przestrzeni OpenGL
        out[i] = glm::vec3(p.x, p.z, -p.y);
    }

    return BoneAnimResult{ out, global };
}

struct SkeletonRenderData {
    GLuint vao{0};
    GLuint vbo{0};
    GLsizei vertex_count{0};
};

SkeletonRenderData update_skeleton_buffer(
    const zenkit::ModelHierarchy& hierarchy,
    const std::vector<glm::vec3>& animated_positions,
    SkeletonRenderData old_data)
{
    std::vector<glm::vec3> line_vertices;

    for (size_t i = 0; i < hierarchy.nodes.size(); ++i)
    {
        const auto& node = hierarchy.nodes[i];
        if (node.parent_index != 0xFFFF && node.parent_index < hierarchy.nodes.size())
        {
            line_vertices.push_back(animated_positions[node.parent_index]);
            line_vertices.push_back(animated_positions[i]);
        }
    }

    if (old_data.vao == 0)
    {
        glGenVertexArrays(1, &old_data.vao);
        glGenBuffers(1, &old_data.vbo);
    }

    old_data.vertex_count = static_cast<GLsizei>(line_vertices.size());

    glBindVertexArray(old_data.vao);
    glBindBuffer(GL_ARRAY_BUFFER, old_data.vbo);
    glBufferData(GL_ARRAY_BUFFER, line_vertices.size() * sizeof(glm::vec3), line_vertices.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return old_data;
}

void free_render_meshes(std::vector<RenderMesh>& meshes) {
    for (auto& m : meshes) {
        if (m.vao) glDeleteVertexArrays(1, &m.vao);
        if (m.vbo) glDeleteBuffers(1, &m.vbo);
        if (m.ebo) glDeleteBuffers(1, &m.ebo);
    }
    meshes.clear();
}


int main(int argc, char** argv) 
{
    if (!glfwInit()) return 1;

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Gothic Animation & Mesh Viewer", nullptr, nullptr);
    if (!window) return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLuint shader_program = create_program();
    AppState app;

    g_app_ptr = &app;
    glfwSetScrollCallback(window, scroll_callback);

    std::string model_name = "DEMON";

    std::string gothic_dir = "/home/mz/.wine/drive_c/Program Files (x86)/JoWood/Gothic II";

    if (argc > 1) model_name = argv[1];
    if (argc > 2) gothic_dir = argv[2];

    LoadedCharacter character = load_character_smart(gothic_dir, model_name);

    if (!character.valid) return EXIT_FAILURE;

    std::vector<RenderMesh> body_meshes;
    std::vector<glm::mat4> bone_mats;
    SkeletonRenderData skel_render{};

    app.model_center = glm::vec3(0.0f, 0.0f, 0.0f);
    app.camera_dist = 300.0f;

    int current_frame = 0;
    int last_frame = -1;
    bool render_wireframe = false;
    bool show_skeleton = true;
    bool opaque_mode = false; 

    while (!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) 
            glfwSetWindowShouldClose(window, true);

        handle_camera_input(window, app);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Kontrola Modelu");
        ImGui::Text("Szkielet: %s", character.script.skeleton.name.c_str());
        ImGui::Text("Liczba kosci: %zu", character.hierarchy.nodes.size());
        ImGui::Separator();

        ImGui::Checkbox("Tryb Siatki (Wireframe)", &render_wireframe);
        ImGui::Checkbox("Pokaz Kosci Szkieletu", &show_skeleton);
        ImGui::Checkbox("Wylacz przezroczystosc", &opaque_mode); 
        ImGui::Separator();

        if (!character.animations.empty())
        {
            const auto& anim = character.animations[0];
            ImGui::Text("Animacja: %s", anim.name.c_str());
            ImGui::SliderInt("Klatka", &current_frame, 0, static_cast<int>(anim.frame_count - 1));
        }

        ImGui::Separator();
        ImGui::SliderFloat("Dystans", &app.camera_dist, 10.0f, 2000.0f);
        ImGui::SliderFloat("Pitch", &app.camera_pitch, -89.0f, 89.0f);
        ImGui::SliderFloat("Yaw", &app.camera_yaw, 0.0f, 360.0f);
        ImGui::End();

        std::vector<glm::vec3> anim_pos;

        // Aktualizujemy siatkę geometryczną tylko po zmianie klatki animacji
        if (current_frame != last_frame || body_meshes.empty())
        {
            last_frame = current_frame;

            if (!character.animations.empty())
            {
                BoneAnimResult anim_res = get_animated_bone_positions(
                    character.hierarchy,
                    character.animations[0],
                    current_frame,
                    character.script.skeleton.name);

                anim_pos = anim_res.positions;
                bone_mats = anim_res.matrices;

                free_render_meshes(body_meshes);

                body_meshes = create_body_meshes(character.mesh, bone_mats);
            }
            else
            {
                size_t num_nodes = character.hierarchy.nodes.size();
                bone_mats.assign(num_nodes, glm::mat4(1.0f));
                anim_pos.resize(num_nodes);

                for (size_t i = 0; i < num_nodes; ++i)
                {
                    const auto& node = character.hierarchy.nodes[i];
                    glm::mat4 local = zenkit_to_glm(node.transform);

                    if (node.parent_index != 0xFFFF && node.parent_index < num_nodes)
                        bone_mats[i] = bone_mats[node.parent_index] * local;
                    else
                        bone_mats[i] = local;

                    glm::vec4 raw = bone_mats[i][3];
                    anim_pos[i] = glm::vec3(raw.x, raw.z, -raw.y);
                }

                if (body_meshes.empty())
                {
                    body_meshes = create_body_meshes(character.mesh, bone_mats); // zamiast identity_matrices
                }
            }

            skel_render = update_skeleton_buffer(character.hierarchy, anim_pos, skel_render);
        }

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float rad_pitch = glm::radians(app.camera_pitch);
        float rad_yaw = glm::radians(app.camera_yaw);

        glm::vec3 cam_pos;
        cam_pos.x = app.model_center.x + app.camera_dist * std::cos(rad_pitch) * std::sin(rad_yaw);
        cam_pos.y = app.model_center.y + app.camera_dist * std::sin(rad_pitch);
        cam_pos.z = app.model_center.z + app.camera_dist * std::cos(rad_pitch) * std::cos(rad_yaw);

        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)w / (float)h, 1.0f, 10000.0f);
        glm::mat4 view = glm::lookAt(cam_pos, app.model_center, glm::vec3(0, 1, 0));
        glm::mat4 model = glm::mat4(1.0f);

        glUseProgram(shader_program);
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, glm::value_ptr(model));

        if (!body_meshes.empty())
        {
            glPolygonMode(GL_FRONT_AND_BACK, render_wireframe ? GL_LINE : GL_FILL);
            glUniform3f(glGetUniformLocation(shader_program, "objectColor"), 0.6f, 0.6f, 0.6f);
            glUniform1i(glGetUniformLocation(shader_program, "useLighting"), true);
            glUniform1i(glGetUniformLocation(shader_program, "opaqueMode"), opaque_mode);

             glDepthMask(opaque_mode ? GL_TRUE : GL_FALSE); 
            for (const auto& m : body_meshes)
            {
                glBindVertexArray(m.vao);
                glDrawElements(GL_TRIANGLES, m.index_count, GL_UNSIGNED_INT, 0);
            }
            glDepthMask(GL_TRUE);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        if (show_skeleton && skel_render.vao && skel_render.vertex_count > 0)
        {
            glLineWidth(2.5f);
            glUniform3f(glGetUniformLocation(shader_program, "objectColor"), 0.0f, 1.0f, 0.4f);
            glUniform1i(glGetUniformLocation(shader_program, "useLighting"), false);

            glBindVertexArray(skel_render.vao);
            glDrawArrays(GL_LINES, 0, skel_render.vertex_count);
            glBindVertexArray(0);
        }

        glm::vec3 mesh_origin = app.model_center;
        glm::vec3 skeleton_origin(0.0f);
        if (!anim_pos.empty()) {
            skeleton_origin = anim_pos[0];
        }

        draw_origins(shader_program, mesh_origin, skeleton_origin, view, proj);
        draw_axis_labels_imgui(view, proj, 150.0f);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        draw_axes(shader_program, 150.0f);

        glfwSwapBuffers(window);
    }

    free_render_meshes(body_meshes);
    if (skel_render.vao) glDeleteVertexArrays(1, &skel_render.vao);
    if (skel_render.vbo) glDeleteBuffers(1, &skel_render.vbo);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}