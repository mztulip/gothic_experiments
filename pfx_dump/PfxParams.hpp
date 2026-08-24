#pragma once

#include <string>
#include <algorithm>
#include <cstdlib>
#include <glm/glm.hpp>
#include <zenkit/addon/daedalus.hh>

struct PfxParams
{
    std::string originDatFile;
    std::string loadedTexturePath;

    float       ppsValue = 0.f;
    std::string visName;

    std::string shpType;
    glm::vec3   shpOffset = glm::vec3(0.f);
    glm::vec3   shpDim    = glm::vec3(0.f);
    bool        shpIsVolume = false;

    std::string dirMode;
    float       dirAngleHead    = 0.f;
    float       dirAngleHeadVar = 0.f;
    float       dirAngleElev    = 0.f;
    float       dirAngleElevVar = 0.f;

    float       velAvg = 0.f;
    float       velVar = 0.f;
    float       lspAvg = 0.f;
    float       lspVar = 0.f;

    glm::vec3   gravity = glm::vec3(0.f);

    glm::vec3   colorStart = glm::vec3(1.f);
    glm::vec3   colorEnd   = glm::vec3(1.f);
    glm::vec2   sizeStart  = glm::vec2(0.f);
    float       sizeEndScale = 1.f;
    float       alphaStart = 1.f;
    float       alphaEnd   = 1.f;

    void clear() {
        originDatFile.clear();
        loadedTexturePath.clear();
        visName.clear();
        shpType.clear();
        dirMode.clear();

        ppsValue = 0.f;
        velAvg = 0.f; velVar = 0.f;
        lspAvg = 0.f; lspVar = 0.f;
        dirAngleHead = 0.f; dirAngleHeadVar = 0.f;
        dirAngleElev = 0.f; dirAngleElevVar = 0.f;

        shpOffset = glm::vec3(0.f);
        shpDim = glm::vec3(0.f);
        gravity = glm::vec3(0.f);
        colorStart = glm::vec3(1.f);
        colorEnd = glm::vec3(1.f);
        sizeStart = glm::vec2(0.f);

        sizeEndScale = 1.f;
        alphaStart = 1.f;
        alphaEnd = 1.f;
        shpIsVolume = false;
    }
};

inline glm::vec3 parseVec3(const std::string& s, glm::vec3 fallback = glm::vec3(0.f))
{
    float v[3] = {fallback.x, fallback.y, fallback.z};
    const char* str = s.c_str();
    for(int i = 0; i < 3; ++i)
    {
        char* next = nullptr;
        float f = std::strtof(str, &next);
        if(str == next) break;
        v[i] = f;
        str = next;
    }
    return glm::vec3(v[0], v[1], v[2]);
}

inline glm::vec2 parseVec2(const std::string& s, glm::vec2 fallback = glm::vec2(0.f))
{
    float v[2] = {fallback.x, fallback.y};
    const char* str = s.c_str();
    for(int i = 0; i < 2; ++i)
    {
        char* next = nullptr;
        float f = std::strtof(str, &next);
        if(str == next) break;
        v[i] = f;
        str = next;
    }
    return glm::vec2(v[0], v[1]);
}

inline PfxParams extractParams(const zenkit::IParticleEffect& p)
{
    PfxParams out;
    out.ppsValue        = p.pps_value;
    out.visName         = p.vis_name_s;

    out.shpType         = p.shp_type_s;
    out.shpOffset       = parseVec3(p.shp_offset_vec_s);
    out.shpDim          = parseVec3(p.shp_dim_s);
    out.shpIsVolume     = p.shp_is_volume != 0;

    out.dirMode         = p.dir_mode_s;
    out.dirAngleHead    = p.dir_angle_head;
    out.dirAngleHeadVar = p.dir_angle_head_var;
    out.dirAngleElev    = p.dir_angle_elev;
    out.dirAngleElevVar = p.dir_angle_elev_var;

    out.velAvg          = p.vel_avg;
    out.velVar          = p.vel_var;
    out.lspAvg          = p.lsp_part_avg > 0.f ? p.lsp_part_avg : 500.f;
    out.lspVar          = p.lsp_part_var;

    out.gravity         = parseVec3(p.fly_gravity_s);

    out.colorStart      = parseVec3(p.vis_tex_color_start_s, glm::vec3(255.f)) / 255.f;
    out.colorEnd        = parseVec3(p.vis_tex_color_end_s,   glm::vec3(255.f)) / 255.f;

    out.sizeStart       = parseVec2(p.vis_size_start_s, glm::vec2(10.f));
    out.sizeEndScale    = p.vis_size_end_scale > 0.f ? p.vis_size_end_scale : 1.f;

    out.alphaStart      = std::clamp(p.vis_alpha_start / 255.f, 0.f, 1.f);
    out.alphaEnd        = std::clamp(p.vis_alpha_end / 255.f,   0.f, 1.f);

    return out;
}