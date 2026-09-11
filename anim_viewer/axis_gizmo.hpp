#pragma once

#include <iostream>        // std::cerr
#include <vector>          // std::vector
#include <cstdio>          // FILE, fopen, fread, fseek, ftell
#include <glm/glm.hpp>     // glm::vec3, glm::mat4
#include <glm/gtc/matrix_transform.hpp>  // glm::translate
#include <glm/gtc/type_ptr.hpp>          // glm::value_ptr

#include <epoxy/gl.h>      // OpenGL: GLuint, glGenBuffers, glBindBuffer, glDrawElements, etc.
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"  // font triangulation


struct FontMesh {
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    GLsizei index_count;
};

FontMesh load_font_mesh(const char* ttf_path, char character, float scale = 1.0f)
{
    // Wczytanie pliku TTF
    FILE* f = fopen(ttf_path, "rb");
    if (!f) {
        std::cerr << "Nie mogę otworzyć fonta: " << ttf_path << "\n";
        return {};
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<unsigned char> buffer(size);
    fread(buffer.data(), 1, size, f);
    fclose(f);

    stbtt_fontinfo font;
    stbtt_InitFont(&font, buffer.data(), stbtt_GetFontOffsetForIndex(buffer.data(), 0));

    stbtt_vertex* vertices;
    int vcount = stbtt_GetCodepointShape(&font, character, &vertices);

    std::vector<glm::vec3> pts;
    std::vector<uint32_t> idx;

    // triangulacja konturów (prosta)
    for (int i = 0; i < vcount; ++i)
    {
        float x = vertices[i].x * scale;
        float y = vertices[i].y * scale;
        pts.push_back(glm::vec3(x, y, 0.0f));
    }

    // prosta triangulacja: fan
    for (int i = 1; i < vcount - 1; ++i)
    {
        idx.push_back(0);
        idx.push_back(i);
        idx.push_back(i + 1);
    }

    stbtt_FreeShape(&font, vertices);

    FontMesh fm;

    glGenVertexArrays(1, &fm.vao);
    glGenBuffers(1, &fm.vbo);
    glGenBuffers(1, &fm.ebo);

    glBindVertexArray(fm.vao);

    glBindBuffer(GL_ARRAY_BUFFER, fm.vbo);
    glBufferData(GL_ARRAY_BUFFER, pts.size() * sizeof(glm::vec3), pts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fm.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    fm.index_count = idx.size();
    return fm;
}

void draw_axes(GLuint shader_program, float axis_len = 100.0f)
{
    // Kolory osi
    glm::vec3 X_color(1.0f, 0.0f, 0.0f); // czerwony
    glm::vec3 Y_color(0.0f, 1.0f, 0.0f); // zielony
    glm::vec3 Z_color(0.0f, 0.0f, 1.0f); // niebieski

    glm::vec3 origin(0.0f);
    glm::vec3 x_end(axis_len, 0.0f, 0.0f);
    glm::vec3 y_end(0.0f, axis_len, 0.0f);
    glm::vec3 z_end(0.0f, 0.0f, axis_len);

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glm::vec3 axis_vertices[] = {
        origin, x_end,
        origin, y_end,
        origin, z_end
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(axis_vertices), axis_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glLineWidth(3.0f);

    // X
    glUniform3f(glGetUniformLocation(shader_program, "objectColor"),
                X_color.x, X_color.y, X_color.z);
    glUniform1i(glGetUniformLocation(shader_program, "useLighting"), false);
    glDrawArrays(GL_LINES, 0, 2);

    // Y
    glUniform3f(glGetUniformLocation(shader_program, "objectColor"),
                Y_color.x, Y_color.y, Y_color.z);
    glDrawArrays(GL_LINES, 2, 2);

    // Z
    glUniform3f(glGetUniformLocation(shader_program, "objectColor"),
                Z_color.x, Z_color.y, Z_color.z);
    glDrawArrays(GL_LINES, 4, 2);

    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

void draw_axis_labels_imgui(
    const glm::mat4& view,
    const glm::mat4& proj,
    float axis_len = 100.0f)
{
    auto project_to_screen = [&](const glm::vec3& p) -> ImVec2
    {
        glm::vec4 clip = proj * view * glm::vec4(p, 1.0f);
        clip /= clip.w;

        float x = (clip.x * 0.5f + 0.5f) * ImGui::GetIO().DisplaySize.x;
        float y = (1.0f - (clip.y * 0.5f + 0.5f)) * ImGui::GetIO().DisplaySize.y;

        return ImVec2(x, y);
    };

    glm::vec3 X_end(axis_len, 0, 0);
    glm::vec3 Y_end(0, axis_len, 0);
    glm::vec3 Z_end(0, 0, axis_len);

    ImDrawList* draw = ImGui::GetForegroundDrawList();

    ImVec2 Xpos = project_to_screen(X_end);
    ImVec2 Ypos = project_to_screen(Y_end);
    ImVec2 Zpos = project_to_screen(Z_end);

    draw->AddText(ImVec2(Xpos.x + 5, Xpos.y + 5), IM_COL32(255, 0, 0, 255), "X");
    draw->AddText(ImVec2(Ypos.x + 5, Ypos.y + 5), IM_COL32(0, 255, 0, 255), "Y");
    draw->AddText(ImVec2(Zpos.x + 5, Zpos.y + 5), IM_COL32(0, 128, 255, 255), "Z");
}


void draw_origin_marker(GLuint shader_program,
                        glm::vec3 world_pos,
                        glm::mat4 view,
                        glm::mat4 proj,
                        const char* label,
                        ImU32 color)
{
    ImDrawList* draw = ImGui::GetForegroundDrawList();

    glm::vec4 clip = proj * view * glm::vec4(world_pos, 1.0f);
    clip /= clip.w;

    float sx = (clip.x * 0.5f + 0.5f) * ImGui::GetIO().DisplaySize.x;
    float sy = (1.0f - (clip.y * 0.5f + 0.5f)) * ImGui::GetIO().DisplaySize.y;

    char buf[128];
    snprintf(buf, sizeof(buf), "%s (%.2f, %.2f, %.2f)",
             label, world_pos.x, world_pos.y, world_pos.z);

    draw->AddText(ImVec2(sx + 5, sy + 5), color, buf);
}

void draw_origins(GLuint shader_program,
                  glm::vec3 mesh_origin,
                  glm::vec3 skeleton_origin,
                  glm::mat4 view,
                  glm::mat4 proj)
{
    draw_origin_marker(shader_program, mesh_origin, view, proj, "M", IM_COL32(255, 255, 0, 255)); // żółty
    draw_origin_marker(shader_program, skeleton_origin, view, proj, "S", IM_COL32(0, 255, 255, 255)); // cyjan
}
