#pragma once
#include "text_renderer.hpp"


static bool isVobInView(
    const glm::vec3& vobPos,
    const Camera& cam,
    float maxDistance = 1500.f,
    float minDot = 0.90f)
{
    glm::vec3 toVob = vobPos - cam.pos;

    float distance = glm::length(toVob);

    // Za daleko
    if(distance > maxDistance)
        return false;

    // Za blisko
    if(distance < 1.f)
        return false;

    toVob /= distance;

    glm::vec3 camDir = glm::normalize(cam.front());

    float dot = glm::dot(camDir, toVob);

    // 0.90 ~= około 26 stopni od środka ekranu
    return dot >= minDot;
}


static void drawHud(TextRenderer& text, float fbw, float fbh, Camera g_cam, const char* hudPreset, float hudRange, float hudDist,
    int g_formulaMode, int g_lightcorrection, int g_tonemap, float g_lightIntensity, bool g_fogEnabled, float g_fogDensity,
    std::vector<LoadedLight>& worldLights,
    glm::mat4& view,
    glm::mat4& proj,
    float fps, bool g_texturesEnabled
    )
{
    // ---- HUD tekstowy w oknie (nie tylko w tytule) ----
    glm::mat4 textOrtho = glm::ortho(0.f, float(fbw), float(fbh), 0.f, -1.f, 1.f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    char line[400];
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

    snprintf(line, sizeof(line), "formula=%s korekcja=%s tonemap=%s  LightIntensity=%.3f tekstury: %s",
             g_formulaMode==0 ? "LINIA" : "SQUARE", g_lightcorrection==0 ? "BRAK" : "OBECNA", 
             g_tonemap ? "ON" : "OFF", 
             g_lightIntensity,
            g_texturesEnabled ? "ON" : "OFF");
    text.drawLine(10.f, ty, line, 150,220,255,255, textOrtho); ty += lh;

    snprintf(line, sizeof(line),
         "mgla=%s  gestosc=%.2f  [F] mgla  [O/P] gestosc  [N] korekcja  [M] formula  [T] tonemap  [Y] tekstury [B] bbox linie",
         g_fogEnabled ? "ON" : "OFF",
         g_fogDensity);
    text.drawLine(10.f, ty, line, 150,255,180,255, textOrtho); ty += lh;

    // ---- etykiety "range: X" nad kazdym swiatlem - tylko w promieniu 1000 od kamery ----
    const float labelMaxDist = 200.f;

    for(auto& l : worldLights)
    {
        float distToCam = glm::length(l.pos - g_cam.pos);
        if(distToCam > labelMaxDist)
            continue;

        glm::vec3 labelPos = l.pos + glm::vec3(0.f, 40.f, 0.f);
        glm::vec2 screenPos;

        if(!worldToScreen(labelPos, view, proj, fbw, fbh, screenPos))
            continue;

        // Convert light color from [0, 1] to [0, 255]
        int r = static_cast<int>(
            glm::clamp(l.color.r, 0.f, 1.f) * 255.f
        );

        int g = static_cast<int>(
            glm::clamp(l.color.g, 0.f, 1.f) * 255.f
        );

        int b = static_cast<int>(
            glm::clamp(l.color.b, 0.f, 1.f) * 255.f
        );

        char rangeLine[64];
        char colorLine[64];
        char typeLine[64];

        snprintf(rangeLine, sizeof(rangeLine),
                "range: %.0f", l.range);

        snprintf(colorLine, sizeof(colorLine),
                "color: (%d,%d,%d)", r, g, b);

        snprintf(typeLine,  sizeof(typeLine),  "type: %s", l.isStatic ? "static" : "dynamic");

        // Center the text horizontally
        float rangeWidth = float(strlen(rangeLine)) * 6.f;
        float colorWidth = float(strlen(colorLine)) * 6.f;
        float typeWidth  = float(strlen(typeLine))  * 6.f;

        text.drawLine(
            screenPos.x - rangeWidth * 0.5f,
            screenPos.y,
            rangeLine,
            255, 255, 255, 255,
            textOrtho
        );

        text.drawLine(
            screenPos.x - colorWidth * 0.5f,
            screenPos.y + 12.f,
            colorLine,
            255, 255, 255,
            255,
            textOrtho
        );

        text.drawLine(
          screenPos.x - typeWidth * 0.5f,
          screenPos.y + 24.f,
          typeLine,
          255, 255, 255, 255,
          textOrtho
      );
    }


    glEnable(GL_DEPTH_TEST);
}