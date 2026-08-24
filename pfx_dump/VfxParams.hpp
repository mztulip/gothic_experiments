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
    std::string visAlphaBlendFunc;
    float       visTexAniFps = 0.0f;
    bool        visTexAniIsLooping = false;

    // Trajektoria i sterowanie
    std::string emTrjMode;
    std::string emTrjOriginNode;
    std::string emTrjTargetNode;
    float       emTrjTargetRange = 0.0f;
    float       emTrjTargetAzi = 0.0f;
    float       emTrjTargetElev = 0.0f;
    std::string emTrjLoopMode;
    std::string emTrjEaseFunc;
    float       emTrjEaseVel = 0.0f;

    // Efekty towarzyszące i kolizje
    std::string emFxCreate;
    float       emFxLifespan = 0.0f;
    int32_t     emCheckCollision = 0;
    float       emFlyGravity = 0.0f;

    // Światło i Dźwięk
    std::string lightPresetName;
    std::string sfxId;
    bool        sfxIsAmbient = false;

    // Skrypty
    std::string userString;

    void clear() {
        *this = VfxParams();
    }
};

inline VfxParams extractVfxParams(const zenkit::IEffectBase& v)
{
    VfxParams out;
    
    // Wizualia
    out.visName            = v.vis_name_s;
    out.visSize            = v.vis_size_s;
    out.visAlpha           = v.vis_alpha;
    out.visAlphaBlendFunc  = v.vis_alpha_blend_func_s;
    out.visTexAniFps       = v.vis_tex_ani_fps;
    out.visTexAniIsLooping = (v.vis_tex_ani_is_looping != 0);

    // Trajektoria
    out.emTrjMode          = v.em_trj_mode_s;
    out.emTrjOriginNode    = v.em_trj_origin_node;
    out.emTrjTargetNode    = v.em_trj_target_node;
    out.emTrjTargetRange   = v.em_trj_target_range;
    out.emTrjTargetAzi     = v.em_trj_target_azi;
    out.emTrjTargetElev    = v.em_trj_target_elev;
    out.emTrjLoopMode      = v.em_trj_loop_mode_s;
    out.emTrjEaseFunc      = v.em_trj_ease_func_s;
    out.emTrjEaseVel       = v.em_trj_ease_vel;

    // Kolizje i Efekty
    out.emFxCreate         = v.em_fx_create_s;
    out.emFxLifespan       = v.em_fx_lifespan;
    out.emCheckCollision   = v.em_check_collision;
    out.emFlyGravity       = v.em_fly_gravity;

    // Audio i Światło
    out.lightPresetName    = v.light_preset_name;
    out.sfxId              = v.sfx_id;
    out.sfxIsAmbient       = (v.sfx_is_ambient != 0);

    // Tablica user_string
    for (int i = 0; i < zenkit::IEffectBase::user_string_count; ++i) {
        if (!v.user_string[i].empty()) {
            if (!out.userString.empty()) out.userString += " | ";
            out.userString += v.user_string[i];
        }
    }

    return out;
}