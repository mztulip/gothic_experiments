#pragma once

#include "VfxParams.hpp"
#include "pfxloader.hpp"
#include <zenkit/DaedalusVm.hh>
#include <zenkit/addon/daedalus.hh>
#include <memory>
#include <string>
#include <vector>
#include <iostream>

class VfxLoader {
public:

static void dumpEffectBase(const zenkit::IEffectBase& v)
{
    std::cout << "\n========== IEffectBase ==========\n";

    std::cout
        << "VIS:\n"
        << "  name=" << v.vis_name_s
        << " size=" << v.vis_size_s
        << " alpha=" << v.vis_alpha
        << " blend=" << v.vis_alpha_blend_func_s
        << " fps=" << v.vis_tex_ani_fps
        << " loop=" << v.vis_tex_ani_is_looping
        << "\n";

    std::cout
        << "TRAJECTORY:\n"
        << "  mode=" << v.em_trj_mode_s
        << " origin=" << v.em_trj_origin_node
        << " target=" << v.em_trj_target_node
        << " range=" << v.em_trj_target_range
        << " azi=" << v.em_trj_target_azi
        << " elev=" << v.em_trj_target_elev
        << "\n";

    std::cout
        << "  keys=" << v.em_trj_num_keys
        << " keysVar=" << v.em_trj_num_keys_var
        << " elevVar=" << v.em_trj_angle_elev_var
        << " headVar=" << v.em_trj_angle_head_var
        << " distVar=" << v.em_trj_key_dist_var
        << "\n";

    std::cout
        << "  loop=" << v.em_trj_loop_mode_s
        << " ease=" << v.em_trj_ease_func_s
        << " easeVel=" << v.em_trj_ease_vel
        << " updateDelay=" << v.em_trj_dyn_update_delay
        << " targetOnly=" << v.em_trj_dyn_update_target_only
        << "\n";

    std::cout
        << "EFFECT:\n"
        << "  create=" << v.em_fx_create_s
        << " investOrigin=" << v.em_fx_invest_origin_s
        << " investTarget=" << v.em_fx_invest_target_s
        << " triggerDelay=" << v.em_fx_trigger_delay
        << " downTrj=" << v.em_fx_create_down_trj
        << "\n";

    std::cout
        << "COLLISION:\n"
        << "  dynAction=" << v.em_action_coll_dyn_s
        << " statAction=" << v.em_action_coll_stat_s
        << " statFx=" << v.em_fx_coll_stat_s
        << " dynFx=" << v.em_fx_coll_dyn_s
        << " statAlign=" << v.em_fx_coll_stat_align_s
        << " dynAlign=" << v.em_fx_coll_dyn_align_s
        << " dynPerc=" << v.em_fx_coll_dyn_perc_s
        << "\n";

    std::cout
        << "PHYSICS:\n"
        << "  lifespan=" << v.em_fx_lifespan
        << " checkCollision=" << v.em_check_collision
        << " adjustShape=" << v.em_adjust_shp_to_origin
        << " nextKeyDuration=" << v.em_invest_next_key_duration
        << " gravity=" << v.em_fly_gravity
        << " rotVel=" << v.em_self_rot_vel_s
        << "\n";

    std::cout
        << "AUDIO/LIGHT:\n"
        << "  light=" << v.light_preset_name
        << " sfx=" << v.sfx_id
        << " ambient=" << v.sfx_is_ambient
        << "\n";

    std::cout
        << "MAGIC:\n"
        << "  assess=" << v.send_assess_magic
        << " secsPerDamage=" << v.secs_per_damage
        << "\n";

    std::cout << "USER:\n";

    for (std::size_t i = 0;
         i < zenkit::IEffectBase::user_string_count;
         ++i)
    {
        std::cout
            << "  [" << i << "] = "
            << v.user_string[i]
            << "\n";
    }

    std::cout
        << "=================================\n";
}


static void inspectInstance(
    zenkit::DaedalusSymbol* instSym,
    const std::shared_ptr<LoadedScript>& sc)
{
    if (!instSym || !sc || !sc->vm)
        return;

    // ============================================================
    // INSTANCE
    // ============================================================

    auto* parentSym =
        sc->vm->find_symbol_by_index(instSym->parent());

    std::string parentName =
        parentSym
            ? parentSym->name()
            : "Nieznana";

    std::cout
        << "\n"
        << "============================================================\n"
        << " INSTANCE\n"
        << "============================================================\n";

    std::cout
        << "  name          = " << instSym->name() << "\n"
        << "  index         = " << instSym->index() << "\n"
        << "  type          = " << static_cast<int>(instSym->type()) << "\n"
        << "  count         = " << instSym->count() << "\n"
        << "  parent_index  = " << instSym->parent() << "\n";


    // ============================================================
    // PARENT
    // ============================================================

    std::cout
        << "\n"
        << "========== DEBUG PARENT ==========\n";

    if (parentSym)
    {
        std::cout
            << "  name          = " << parentSym->name() << "\n"
            << "  index         = " << parentSym->index() << "\n"
            << "  type          = " << static_cast<int>(parentSym->type()) << "\n"
            << "  count         = " << parentSym->count() << "\n"
            << "  parent_index  = " << parentSym->parent() << "\n";
    }
    else
    {
        std::cout
            << "  parentSym     = nullptr\n";
    }


    // ============================================================
    // GRANDPARENT
    // ============================================================

    auto* grandParent =
        parentSym
            ? sc->vm->find_symbol_by_index(parentSym->parent())
            : nullptr;

    std::cout
        << "\n"
        << "========== DEBUG GRANDPARENT ==========\n";

    if (grandParent)
    {
        std::cout
            << "  name          = " << grandParent->name() << "\n"
            << "  index         = " << grandParent->index() << "\n"
            << "  type          = " << static_cast<int>(grandParent->type()) << "\n"
            << "  count         = " << grandParent->count() << "\n"
            << "  parent_index  = " << grandParent->parent() << "\n";
    }
    else
    {
        std::cout
            << "  grandParent   = nullptr\n";
    }

    std::cout
        << "========================================\n";


    // ============================================================
    // BRAK KLASY
    // ============================================================

    if (!parentSym)
    {
        std::cout
            << "\n"
            << "  [VFX Loader] Nieznana klasa!\n"
            << "  Nie można określić pól klasy.\n"
            << std::endl;

        return;
    }


    // ============================================================
    // ROZPOZNANIE KLASY
    // ============================================================

    const bool isParticleKey =
        parentName == "C_PARTICLEFXEMITKEY";

    const bool isEffectBase =
        parentName == "C_XIVISUALFX" ||
        parentName == "C_XIVISUALFX_D" ||
        parentName == "CFX_BASE_PROTO";


    if (isParticleKey)
    {
        std::cout
            << "\n"
            << "  [VFX Loader] Rozpoznano:\n"
            << "  C_PARTICLEFXEMITKEY\n";

        auto inst =
            std::make_shared<zenkit::IParticleEffectEmitKey>();

        std::cout
            << "  [DEBUG] IParticleEffectEmitKey utworzony\n"
            << "  [DEBUG] init_instance pominięte\n";
    }
    else if (isEffectBase)
    {
        std::cout
            << "\n"
            << "  [VFX Loader] Rozpoznano:\n"
            << "  "
            << parentName
            << "\n";

        auto inst =
            std::make_shared<zenkit::IEffectBase>();

        try
        {
            sc->vm->init_instance(inst, instSym);

            std::cout
                << "  [DEBUG] IEffectBase utworzony\n"
                << "  [DEBUG] init_instance OK\n";
        }
        catch (const std::exception& e)
        {
            std::cerr
                << "  [ERROR] init_instance: "
                << e.what()
                << "\n";
        }
    }
    else
    {
        // ========================================================
        // NIEZNANA KLASA
        // ========================================================

        std::cout
            << "\n"
            << "  [VFX Loader] NIEZNANA KLASA!\n"
            << "  class = "
            << parentName
            << "\n";

        std::cout
            << "  [VFX Loader] Wypisuję wszystkie pola klasy.\n";
    }


    // ============================================================
    // WSZYSTKIE POLA KLASY
    //
    // To jest wspólne dla KAŻDEJ klasy.
    // Dzięki temu nawet nieznane klasy są w pełni widoczne.
    // ============================================================

    std::cout
        << "\n"
        << "  ===== POLA KLASY "
        << parentName
        << " =====\n";

    std::size_t found = 0;

    const auto classIndex =
        parentSym->index();

    const auto fieldCount =
        parentSym->count();

    const auto firstField =
        classIndex + 1;

    const auto lastField =
        firstField + fieldCount;


    for (auto i = firstField;
         i < lastField;
         ++i)
    {
        auto* field =
            sc->vm->find_symbol_by_index(i);

        if (!field)
        {
            std::cout
                << "  [ERROR] Brak symbolu index="
                << i
                << "\n";

            continue;
        }

        ++found;

        std::cout
            << "  FIELD #"
            << found

            << "  index="
            << field->index()

            << "  type="
            << static_cast<int>(field->type())

            << "  count="
            << field->count()

            << "  name="
            << field->name()

            << "  parent="
            << field->parent()

            << "\n";
    }


    std::cout
        << "\n"
        << "  znaleziono pól = "
        << found
        << " / "
        << fieldCount
        << "\n"

        << "  ============================================\n"
        << std::endl;
}


    static bool tryLoadVfx(const std::string& name, const std::vector<std::shared_ptr<LoadedScript>>& scripts, VfxParams& outParams)
    {
        std::cout<<"Trying to load vfx"<<std::endl;

        for (const auto& sc : scripts) {
            if (!sc || !sc->vm) continue;
            std::cout<<"Checking file: "<<*sc<<std::endl;
            auto* sym = sc->vm->find_symbol_by_name(name);
            if (!sym) continue;

            std::cout << "[DEBUG] Znaleziono symbol: "
                << sym->name()
                << " w "
                << sc->fileName
                << std::endl;


            // Sprawdzamy czy symbol to faktycznie C_XIVISUALFX / C_PARTICLEFX_EMITTER
            auto* parentSym = sc->vm->find_symbol_by_index(sym->parent());
            std::string parentName = parentSym ? parentSym->name() : "Brak";
            std::cout << "[VFX Loader] Symbol: " << sym->name() 
                << " | Typ rodzica: " << parentName << std::endl;

            // Sprawdzamy czy symbol to klasa VFX lub klucz PFX/VFX
            if (parentName != "C_XIVISUALFX" && 
                parentName != "C_PARTICLEFXEMITKEY" && 
                parentName != "C_XIVISUALFX_D" &&
                parentName != "CFX_BASE_PROTO")
            {
                continue; // Pomijamy symbole, które nie dziedziczą po klasach VFX
            }

            try {
                inspectInstance(sym,sc);
                // auto vfx = std::make_shared<zenkit::IEffectBase>();
                // sc->vm->init_instance(vfx, sym);

                // std::string childPfxName = vfx->em_fx_create_s;

                // // Jeśli VFX zawiera podpięty PFX (np. em_fx_create_s), ładujemy go
                // // if (!childPfxName.empty() && childPfxName != name) {
                // //     if (PfxLoader::tryLoadPfx(childPfxName, scripts, outParams)) {
                // //         outParams.originDatFile = sc->fileName + " -> " + childPfxName;
                // //         return true;
                // //     }
                // // }

                // // Awaryjnie wyciągamy teksturę/wizualia bezpośrednio z VFX
                // outParams = VfxParams();
                // outParams.visName = vfx->vis_name_s;
                // outParams.originDatFile = sc->fileName;
                // return true;
            } catch (const std::exception& e) 
            {
                std::cerr << "[VFX Loader Error] " << e.what() << std::endl;
                continue;
            }
        }

        // for (const auto& sc : scripts)
        // {
        //     if (sc && sc->fileName == "VISUALFX.DAT")
        //     {
        //         VfxLoader::dumpClasses(sc);
        //         break;
        //     }
        // }
         std::cout<<"Vfx loading failed"<<std::endl;
        return false;
    }
};