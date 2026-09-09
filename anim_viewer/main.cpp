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

    FragColor = vec4(ambient + diffuse, 1.0);
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

std::vector<RenderMesh> create_body_meshes(const zenkit::ModelMesh& model_mesh)
{
    std::vector<RenderMesh> meshes;

    std::cout
    << "[DEBUG] ZenKit meshes: "
    << model_mesh.meshes.size()
    << '\n';

    for (const auto& skin : model_mesh.meshes)
    {
        const auto& mesh = skin.mesh;
        std::vector<glm::vec3> positions;
        positions.reserve(mesh.positions.size());

        for (const auto& p : mesh.positions)
            positions.push_back(glm::vec3(p.x, p.z, -p.y));

        for (const auto& sub : mesh.sub_meshes)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            std::cout
                << "[DEBUG] positions=" << mesh.positions.size()
                << " wedges=" << sub.wedges.size()
                << " triangles=" << sub.triangles.size()
                << '\n';


            vertices.reserve(sub.wedges.size());
            indices.reserve(sub.triangles.size() * 3);

            for (const auto& wedge : sub.wedges)
            {
                glm::vec3 pos(0.0f);
                if (wedge.index < positions.size())
                    pos = positions[wedge.index];

                vertices.push_back({
                    pos,
                    glm::vec3(wedge.normal.x, wedge.normal.z, -wedge.normal.y)
                });
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
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rm.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

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

std::vector<glm::vec3> get_animated_bone_positions(
    const zenkit::ModelHierarchy& hierarchy,
    const zenkit::ModelAnimation& anim,
    int frame)
{
    size_t num_nodes = hierarchy.nodes.size();
    std::vector<glm::mat4> global_transforms(num_nodes, glm::mat4(1.0f));
    std::vector<glm::vec3> positions(num_nodes, glm::vec3(0.0f));

    if (num_nodes == 0) return positions;

    uint32_t frame_idx = std::clamp(frame, 0, (int)anim.frame_count - 1);

    for (size_t i = 0; i < num_nodes; ++i)
    {
        const auto& node = hierarchy.nodes[i];

        glm::mat4 local_transform(
            node.transform[0][0], node.transform[0][1], node.transform[0][2], node.transform[0][3],
            node.transform[1][0], node.transform[1][1], node.transform[1][2], node.transform[1][3],
            node.transform[2][0], node.transform[2][1], node.transform[2][2], node.transform[2][3],
            node.transform[3][0], node.transform[3][1], node.transform[3][2], node.transform[3][3]
        );

        size_t sample_idx = frame_idx * num_nodes + i;
        if (sample_idx < anim.samples.size())
        {
            const auto& sample = anim.samples[sample_idx];

            glm::quat rot(sample.rotation.w, sample.rotation.x, sample.rotation.y, sample.rotation.z);
            glm::mat4 rot_matrix = glm::mat4_cast(rot);

            glm::mat4 trans_matrix = glm::translate(glm::mat4(1.0f), 
                glm::vec3(sample.position.x, sample.position.y, sample.position.z));

            local_transform = trans_matrix * rot_matrix;
        }

        if (node.parent_index != 0xFFFF && node.parent_index < num_nodes)
        {
            global_transforms[i] = global_transforms[node.parent_index] * local_transform;
        }
        else
        {
            global_transforms[i] = local_transform;
        }

        glm::vec4 pos = global_transforms[i][3];
        positions[i] = glm::vec3(pos.x, pos.z, -pos.y);
    }

    return positions;
}

struct SkeletonRenderData {
    GLuint vao{0}, vbo{0};
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

    GLuint shader_program = create_program();
    AppState app;

    std::string model_name = "ALLIGATOR";
    std::string gothic_dir = "/home/mz/.wine/drive_c/Program Files (x86)/JoWood/Gothic II";

    if (argc > 1) model_name = argv[1];
    if (argc > 2) gothic_dir = argv[2];

    LoadedCharacter character = load_character_smart(gothic_dir, model_name);

    if (!character.valid) return EXIT_FAILURE;

    std::vector<RenderMesh> body_meshes;
    if (character.has_mesh)
    {
        body_meshes = create_body_meshes(character.mesh);
        std::cout << "[DEBUG] body_meshes: " << body_meshes.size() << '\n';

        for (const auto& m : body_meshes)
        {
            std::cout
                << "[DEBUG] VAO=" << m.vao
                << " VBO=" << m.vbo
                << " EBO=" << m.ebo
                << " indices=" << m.index_count
                << '\n';
        }

    }

    SkeletonRenderData skel_render{};

    app.model_center = glm::vec3(0.0f, 0.0f, 0.0f);
    app.camera_dist = 300.0f;

    int current_frame = 0;
    bool render_wireframe = false;
    bool show_skeleton = true;

    while (!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) 
            glfwSetWindowShouldClose(window, true);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Kontrola Modelu");
        ImGui::Text("Szkielet: %s", character.script.skeleton.name.c_str());
        ImGui::Text("Liczba kosci: %zu", character.hierarchy.nodes.size());
        ImGui::Separator();

        ImGui::Checkbox("Tryb Siatki (Wireframe)", &render_wireframe);
        ImGui::Checkbox("Pokaz Kosci Szkieletu", &show_skeleton);
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
        if (!character.animations.empty())
            anim_pos = get_animated_bone_positions(character.hierarchy, character.animations[0], current_frame);
        else
        {
            for (const auto& node : character.hierarchy.nodes)
                anim_pos.push_back(glm::vec3(node.transform[3][0], node.transform[3][2], -node.transform[3][1]));
        }

        skel_render = update_skeleton_buffer(character.hierarchy, anim_pos, skel_render);

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

            for (const auto& m : body_meshes)
            {
                   std::cout
                << "[DRAW] vao=" << m.vao
                << " indices=" << m.index_count
                << '\n';

                glBindVertexArray(m.vao);
                glDrawElements(GL_TRIANGLES, m.index_count, GL_UNSIGNED_INT, 0);
            }
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

        ImGui::Render();
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