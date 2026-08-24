#pragma once

#include <string>
#include <cstdlib>
#include <glm/glm.hpp>
#include <zenkit/addon/daedalus.hh>

struct VfxParams
{
    std::string originDatFile;
    std::string loadedTexturePath;

    // Pole wizualne (Model .3DS, Morph .MMS lub .PFX)
    std::string visName;
    std::string visSize;
    std::string visAlpha;

    // Trajektoria i sterowanie
    std::string emTrjMode;
    std::string emTrjOriginNode;
    std::string emTrjTargetNode;
    std::string emTrjTargetPos;
    std::string emCheckCollision;

    float       emTrjFXDynScale = 1.0f;
    float       emScaleDuration = 0.0f;
    float       emPfxDelay = 0.0f;

    // Światło (Lighting)
    std::string lightPresetName;
    std::string lightRange;
    std::string lightColor;

    // Skrypty i dźwięki
    std::string userString;

    void clear() {
        *this = VfxParams();
    }
};

inline VfxParams extractVfxParams(const zenkit::IEffectBase& v)
{
    VfxParams out;
    out.visName          = v.vis_name_s;
    out.visSize          = v.vis_size_s;
    out.visAlpha         = v.vis_alpha_s;

    out.emTrjMode        = v.em_trj_mode_s;
    out.emTrjOriginNode  = v.em_trj_origin_node_s;
    out.emTrjTargetNode  = v.em_trj_target_node_s;
    out.emTrjTargetPos   = v.em_trj_target_pos_s;
    out.emCheckCollision = v.em_check_collision_s;

    out.emTrjFXDynScale  = v.em_trj_fx_dyn_scale;
    out.emScaleDuration  = v.em_scale_duration;
    out.emPfxDelay       = v.em_pfx_delay;

    out.lightPresetName  = v.light_preset_name_s;
    out.lightRange       = v.light_range_s;
    out.lightColor       = v.light_color_s;

    for (int i = 0; i < 5; ++i) {
        if (!v.user_string[i].empty()) {
            if (!out.userString.empty()) out.userString += " | ";
            out.userString += v.user_string[i];
        }
    }

    return out;
}