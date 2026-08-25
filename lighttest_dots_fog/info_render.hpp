#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <epoxy/gl.h>
#include <cstdint>
#include <string>

#include "camera.hpp"
#include "stb_easy_font.h"
#include "shader_utils.hpp"
#include "light.hpp"

// ---------------------------------------------------------------------
// Prosty shader 2D (ortho, przestrzen ekranu w pikselach) do tekstu HUD
// rysowanego przez stb_easy_font - format wierzcholka: pos(vec3)+color(rgba8)
// ---------------------------------------------------------------------
static const char* TEXT_VERT_SRC = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;

uniform mat4 uOrtho;
out vec4 vColor;

void main() {
  vColor = aColor;
  gl_Position = uOrtho * vec4(aPos, 1.0);
  }
)GLSL";

static const char* TEXT_FRAG_SRC = R"GLSL(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() { FragColor = vColor; }
)GLSL";

// ---------------------------------------------------------------------
// Tekst na ekranie (HUD) - stb_easy_font generuje geometrie liter jako
// kwady (pos xyz + kolor rgba8, interleaved, 16B/wierzcholek). OpenGL 3.3
// core nie ma GL_QUADS, wiec trojkatujemy przez wspolny bufor indeksow
// (kazdy kwad i: wierzcholki 4i..4i+3 -> trojkaty (0,1,2)(0,2,3)).
// ---------------------------------------------------------------------
struct TextRenderer {
  GLuint prog = 0, vao = 0, vbo = 0, ebo = 0;
  std::vector<char> cpuBuf;
  int    maxQuads = 0;

  void init(int maxQuadsIn = 8192) {
    maxQuads = maxQuadsIn;
    cpuBuf.resize(size_t(maxQuads)*4*16); // 4 wierzcholki/kwad * 16B/wierzcholek

    GLuint vs = compileShader(GL_VERTEX_SHADER, TEXT_VERT_SRC);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, TEXT_FRAG_SRC);
    prog = linkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    std::vector<uint32_t> indices(size_t(maxQuads)*6);
    for(int q = 0; q < maxQuads; ++q) {
      uint32_t base = uint32_t(q)*4;
      indices[q*6+0] = base+0; indices[q*6+1] = base+1; indices[q*6+2] = base+2;
      indices[q*6+3] = base+0; indices[q*6+4] = base+2; indices[q*6+5] = base+3;
      }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(cpuBuf.size()), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(indices.size()*sizeof(uint32_t)), indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void*)12);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    }

  // rysuje jedna linie tekstu, (x,y) w pikselach od lewego-gornego rogu
  void drawLine(float x, float y, const std::string& text,
                unsigned char r, unsigned char g, unsigned char b, unsigned char a,
                const glm::mat4& ortho) {
    unsigned char color[4] = {r,g,b,a};
    int numQuads = stb_easy_font_print(x, y, const_cast<char*>(text.c_str()), color,
                                        cpuBuf.data(), int(cpuBuf.size()));
    if(numQuads<=0) return;
    if(numQuads>maxQuads) numQuads = maxQuads; // bezpiecznik, gdyby tekst byl za dlugi

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, GLsizeiptr(numQuads)*4*16, cpuBuf.data());

    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"uOrtho"), 1, GL_FALSE, glm::value_ptr(ortho));
    glDrawElements(GL_TRIANGLES, numQuads*6, GL_UNSIGNED_INT, (void*)0);
    }
  };



// Rzutuje punkt swiata na wspolrzedne ekranowe (piksele, origin lewy-gorny,
// zgodnie z konwencja uzywana przez TextRenderer/glm::ortho w HUD).
// Zwraca false, jesli punkt jest za kamera lub poza ekranem - wtedy nie rysujemy etykiety.
static bool worldToScreen(const glm::vec3& worldPos,
                           const glm::mat4& view, const glm::mat4& proj,
                           int fbw, int fbh, glm::vec2& outScreen)
{
  glm::vec4 clip = proj * view * glm::vec4(worldPos, 1.0f);
  if(clip.w <= 0.01f) return false; // za kamera / zbyt blisko plaszczyzny
  glm::vec3 ndc = glm::vec3(clip) / clip.w;
  if(ndc.x < -1.f || ndc.x > 1.f || ndc.y < -1.f || ndc.y > 1.f) return false; // poza kadrem

  outScreen.x = (ndc.x * 0.5f + 0.5f) * float(fbw);
  outScreen.y = (1.f - (ndc.y * 0.5f + 0.5f)) * float(fbh); // Y odwrocone (ekran rosnie w dol)
  return true;
}

static void drawHud(TextRenderer& text, float fbw, float fbh, Camera g_cam, const char* hudPreset, float hudRange, float hudDist,
    int g_formulaMode, int g_lightcorrection, int g_tonemap, int g_lightIntensity, bool g_fogEnabled, float g_fogDensity,
    std::vector<LoadedLight>& worldLights,
    glm::mat4& view,
    glm::mat4& proj,
    float fps
    )
{
    // ---- HUD tekstowy w oknie (nie tylko w tytule) ----
    glm::mat4 textOrtho = glm::ortho(0.f, float(fbw), float(fbh), 0.f, -1.f, 1.f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    char line[256];
    float ty = 10.f;
    const float lh = 14.f; // odstep miedzy liniami

        // ---- FPS na samej gorze, dobrze widoczne ----
    snprintf(line, sizeof(line), "FPS: %.1f  (%.2f ms/klatke)", fps, fps > 0.f ? 1000.f/fps : 0.f);
    text.drawLine(10.f, ty, line, 255,255,100,255, textOrtho); ty += lh;

    snprintf(line, sizeof(line), "kamera: x=%.1f  y=%.1f  z=%.1f", g_cam.pos.x, g_cam.pos.y, g_cam.pos.z);
    text.drawLine(10.f, ty, line, 255,255,255,255, textOrtho); ty += lh;

    snprintf(line, sizeof(line), "yaw=%.1f  pitch=%.1f", g_cam.yaw, g_cam.pitch);
    text.drawLine(10.f, ty, line, 200,200,200,255, textOrtho); ty += lh;

    snprintf(line, sizeof(line), "najblizsze swiatlo: %s  Range=%.0f ", hudPreset, hudRange);
    text.drawLine(10.f, ty, line, 255,220,150,255, textOrtho); ty += lh;

    snprintf(line, sizeof(line), "d=%.1f  (d/Range=%.1f%%)", hudDist, 100.f*hudDist/hudRange);
    text.drawLine(10.f, ty, line, 255,220,150,255, textOrtho); ty += lh;

    snprintf(line, sizeof(line), "formula=%s korekcja=%s tonemap=%s  LightIntensity=%.3f",
             g_formulaMode==0 ? "LINIA" : "OBECNA", g_lightcorrection==0 ? "BRAK" : "OBECNA", g_tonemap ? "ON" : "OFF", g_lightIntensity);
    text.drawLine(10.f, ty, line, 150,220,255,255, textOrtho); ty += lh;

    snprintf(line, sizeof(line), "mgla=%s  gestosc=%.2f  [F] toggle [O/P] gestosc",
         g_fogEnabled ? "ON" : "OFF", g_fogDensity);
    text.drawLine(10.f, ty, line, 150,255,180,255, textOrtho); ty += lh;

    // ---- etykiety "range: X" nad kazdym swiatlem - tylko w promieniu 1000 od kamery ----
    const float labelMaxDist = 1000.f;

    for(auto& l : worldLights)
    {
        float distToCam = glm::length(l.pos - g_cam.pos);
        if(distToCam > labelMaxDist) continue; // za daleko - pomijamy etykiete

        glm::vec3 labelPos = l.pos + glm::vec3(0.f, 40.f, 0.f);
        glm::vec2 screenPos;
        if(worldToScreen(labelPos, view, proj, fbw, fbh, screenPos))
        {
            char rangeLine[64];
            snprintf(rangeLine, sizeof(rangeLine), "range: %.0f", l.range);
            float textWidth = float(strlen(rangeLine)) * 6.f;
            text.drawLine(screenPos.x - textWidth * 0.5f, screenPos.y, rangeLine,
                        255, 255, 0, 255, textOrtho);
        }
    }

    glEnable(GL_DEPTH_TEST);
}