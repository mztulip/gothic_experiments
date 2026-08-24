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
        << "  name          = "
        << instSym->name()
        << "\n"

        << "  index         = "
        << instSym->index()
        << "\n"

        << "  type          = "
        << static_cast<int>(instSym->type())
        << "\n"

        << "  count         = "
        << instSym->count()
        << "\n"

        << "  parent_index  = "
        << instSym->parent()
        << "\n";


    // ============================================================
    // DEBUG PARENT
    // ============================================================

    std::cout
        << "\n"
        << "========== DEBUG PARENT ==========\n";

    if (parentSym)
    {
        std::cout
            << "  name          = "
            << parentSym->name()
            << "\n"

            << "  index         = "
            << parentSym->index()
            << "\n"

            << "  type          = "
            << static_cast<int>(parentSym->type())
            << "\n"

            << "  count         = "
            << parentSym->count()
            << "\n"

            << "  parent_index  = "
            << parentSym->parent()
            << "\n";
    }
    else
    {
        std::cout
            << "  parentSym     = nullptr\n";
    }


    // ============================================================
    // DEBUG GRANDPARENT
    // ============================================================

    std::cout
        << "\n"
        << "========== DEBUG GRANDPARENT ==========\n";

    if (parentSym)
    {
        auto* grandParent =
            sc->vm->find_symbol_by_index(
                parentSym->parent()
            );

        if (grandParent)
        {
            std::cout
                << "  name          = "
                << grandParent->name()
                << "\n"

                << "  index         = "
                << grandParent->index()
                << "\n"

                << "  type          = "
                << static_cast<int>(grandParent->type())
                << "\n"

                << "  count         = "
                << grandParent->count()
                << "\n"

                << "  parent_index  = "
                << grandParent->parent()
                << "\n";
        }
        else
        {
            std::cout
                << "  grandParent   = nullptr\n";
        }
    }

    std::cout
        << "========================================\n";


    // ============================================================
    // BRAK PARENT
    // ============================================================

    if (!parentSym)
    {
        std::cout
            << "[VFX Loader] Brak parentSym - koniec inspekcji\n";

        return;
    }


    // ============================================================
    // C_PARTICLEFXEMITKEY
    // ============================================================

    if (parentName == "C_PARTICLEFXEMITKEY")
    {
        std::cout
            << "\n"
            << "  [VFX Loader] Rozpoznano:\n"
            << "  C_PARTICLEFXEMITKEY\n";

        std::cout
            << "  [DEBUG] IParticleEffectEmitKey NIE jest inicjalizowany\n"
            << "  [DEBUG] Wypisujemy tylko symbole pól klasy\n";


        // ========================================================
        // INFORMACJE O KLASIE
        // ========================================================

        const auto classIndex =
            parentSym->index();

        const auto fieldCount =
            parentSym->count();

        std::cout
            << "\n"
            << "  ===== POLA C_PARTICLEFXEMITKEY =====\n"

            << "  class index = "
            << classIndex
            << "\n"

            << "  class count = "
            << fieldCount
            << "\n";


        // ========================================================
        // POLA KLASY
        //
        // C_PARTICLEFXEMITKEY:
        //
        // index 2 = klasa
        // index 3..34 = 32 pola
        //
        // Nie szukamy po nazwie.
        // Idziemy bezpośrednio po indeksach.
        // ========================================================

        const auto firstField =
            classIndex + 1;

        const auto lastField =
            firstField + fieldCount;


        std::size_t found = 0;


        for (auto i = firstField;
             i < lastField;
             ++i)
        {
            auto* field =
                sc->vm->find_symbol_by_index(i);

            if (!field)
            {
                std::cout
                    << "\n"
                    << "  [ERROR] Brak symbolu dla index="
                    << i
                    << "\n";

                continue;
            }


            // ====================================================
            // DODATKOWA WERYFIKACJA
            // ====================================================

            if (field->parent() != classIndex)
            {
                std::cout
                    << "\n"
                    << "  [WARNING] Symbol index="
                    << i
                    << " nie ma oczekiwanego parenta!\n"

                    << "      expected parent = "
                    << classIndex
                    << "\n"

                    << "      actual parent   = "
                    << field->parent()
                    << "\n";
            }


            ++found;


            // ====================================================
            // PRINT POLA
            // ====================================================

            std::cout
                << "\n"
                << "  FIELD #"
                << found
                << "\n"

                << "    index  = "
                << field->index()
                << "\n"

                << "    name   = "
                << field->name()
                << "\n"

                << "    type   = "
                << static_cast<int>(field->type())
                << "\n"

                << "    count  = "
                << field->count()
                << "\n"

                << "    parent = "
                << field->parent()
                << "\n";
        }


        // ========================================================
        // PODSUMOWANIE
        // ========================================================

        std::cout
            << "\n"
            << "  --------------------------------------\n"

            << "  [DEBUG] Znaleziono pól: "
            << found
            << " / "
            << fieldCount
            << "\n"

            << "  --------------------------------------\n"

            << "  ===== KONIEC C_PARTICLEFXEMITKEY =====\n"
            << std::endl;


        return;
    }


    // ============================================================
    // CFX_BASE_PROTO
    // ============================================================

    if (parentName == "CFX_BASE_PROTO")
    {
        std::cout
            << "\n"
            << "  [VFX Loader] Rozpoznano:\n"
            << "  CFX_BASE_PROTO\n";

        std::cout
            << "  [DEBUG] Próba utworzenia IEffectBase...\n";

        auto inst =
            std::make_shared<zenkit::IEffectBase>();

        std::cout
            << "  [DEBUG] IEffectBase utworzony\n";


        // ========================================================
        // INIT
        // ========================================================

        try
        {
            sc->vm->init_instance(
                inst,
                instSym
            );

            std::cout
                << "  [DEBUG] init_instance OK\n";
        }
        catch (const std::exception& e)
        {
            std::cerr
                << "  [ERROR] init_instance: "
                << e.what()
                << "\n";

            return;
        }


        // ========================================================
        // IEffectBase
        // ========================================================

        std::cout
            << "\n"
            << "  ===== IEffectBase =====\n"

            << "  vis_name_s                 = \""
            << inst->vis_name_s
            << "\"\n"

            << "  vis_size_s                 = \""
            << inst->vis_size_s
            << "\"\n"

            << "  vis_alpha                  = "
            << inst->vis_alpha
            << "\n"

            << "  vis_alpha_blend_func_s     = \""
            << inst->vis_alpha_blend_func_s
            << "\"\n"

            << "  vis_tex_ani_fps            = "
            << inst->vis_tex_ani_fps
            << "\n"

            << "  vis_tex_ani_is_looping     = "
            << inst->vis_tex_ani_is_looping
            << "\n"

            << "  em_trj_mode_s              = \""
            << inst->em_trj_mode_s
            << "\"\n"

            << "  em_trj_origin_node         = \""
            << inst->em_trj_origin_node
            << "\"\n"

            << "  em_trj_target_node         = \""
            << inst->em_trj_target_node
            << "\"\n"

            << "  em_trj_target_range        = "
            << inst->em_trj_target_range
            << "\n"

            << "  em_trj_target_azi          = "
            << inst->em_trj_target_azi
            << "\n"

            << "  em_trj_target_elev         = "
            << inst->em_trj_target_elev
            << "\n"

            << "  em_trj_num_keys            = "
            << inst->em_trj_num_keys
            << "\n"

            << "  em_trj_num_keys_var        = "
            << inst->em_trj_num_keys_var
            << "\n"

            << "  em_trj_angle_elev_var      = "
            << inst->em_trj_angle_elev_var
            << "\n"

            << "  em_trj_angle_head_var      = "
            << inst->em_trj_angle_head_var
            << "\n"

            << "  em_trj_key_dist_var        = "
            << inst->em_trj_key_dist_var
            << "\n"

            << "  em_trj_loop_mode_s         = \""
            << inst->em_trj_loop_mode_s
            << "\"\n"

            << "  em_trj_ease_func_s         = \""
            << inst->em_trj_ease_func_s
            << "\"\n"

            << "  em_trj_ease_vel            = "
            << inst->em_trj_ease_vel
            << "\n"

            << "  em_trj_dyn_update_delay    = "
            << inst->em_trj_dyn_update_delay
            << "\n"

            << "  em_trj_dyn_update_target_only = "
            << inst->em_trj_dyn_update_target_only
            << "\n"

            << "  em_fx_create_s              = \""
            << inst->em_fx_create_s
            << "\"\n"

            << "  em_fx_invest_origin_s       = \""
            << inst->em_fx_invest_origin_s
            << "\"\n"

            << "  em_fx_invest_target_s       = \""
            << inst->em_fx_invest_target_s
            << "\"\n"

            << "  em_fx_trigger_delay         = "
            << inst->em_fx_trigger_delay
            << "\n"

            << "  em_fx_create_down_trj       = "
            << inst->em_fx_create_down_trj
            << "\n"

            << "  em_action_coll_dyn_s        = \""
            << inst->em_action_coll_dyn_s
            << "\"\n"

            << "  em_action_coll_stat_s       = \""
            << inst->em_action_coll_stat_s
            << "\"\n"

            << "  em_fx_coll_stat_s           = \""
            << inst->em_fx_coll_stat_s
            << "\"\n"

            << "  em_fx_coll_dyn_s            = \""
            << inst->em_fx_coll_dyn_s
            << "\"\n"

            << "  em_fx_coll_stat_align_s     = \""
            << inst->em_fx_coll_stat_align_s
            << "\"\n"

            << "  em_fx_coll_dyn_align_s      = \""
            << inst->em_fx_coll_dyn_align_s
            << "\"\n"

            << "  em_fx_lifespan              = "
            << inst->em_fx_lifespan
            << "\n"

            << "  em_check_collision          = "
            << inst->em_check_collision
            << "\n"

            << "  em_adjust_shp_to_origin     = "
            << inst->em_adjust_shp_to_origin
            << "\n"

            << "  em_invest_next_key_duration = "
            << inst->em_invest_next_key_duration
            << "\n"

            << "  em_fly_gravity              = "
            << inst->em_fly_gravity
            << "\n"

            << "  em_self_rot_vel_s           = \""
            << inst->em_self_rot_vel_s
            << "\"\n";


        // ========================================================
        // USER STRING
        // ========================================================

        for (std::size_t i = 0;
             i < zenkit::IEffectBase::user_string_count;
             ++i)
        {
            std::cout
                << "  user_string["
                << i
                << "] = \""
                << inst->user_string[i]
                << "\"\n";
        }


        // ========================================================
        // RESZTA
        // ========================================================

        std::cout
            << "  light_preset_name       = \""
            << inst->light_preset_name
            << "\"\n"

            << "  sfx_id                  = \""
            << inst->sfx_id
            << "\"\n"

            << "  sfx_is_ambient          = "
            << inst->sfx_is_ambient
            << "\n"

            << "  send_assess_magic       = "
            << inst->send_assess_magic
            << "\n"

            << "  secs_per_damage         = "
            << inst->secs_per_damage
            << "\n"

            << "  em_fx_coll_dyn_perc_s   = \""
            << inst->em_fx_coll_dyn_perc_s
            << "\"\n"

            << "  =============================\n"
            << std::endl;

        return;
    }


    // ============================================================
    // C_XIVISUALFX / C_XIVISUALFX_D
    // ============================================================

    if (parentName == "C_XIVISUALFX" ||
        parentName == "C_XIVISUALFX_D")
    {
        std::cout
            << "\n"
            << "  [VFX Loader] Rozpoznano:\n"
            << "  "
            << parentName
            << "\n";

        std::cout
            << "  [DEBUG] Tworzenie IEffectBase...\n";

        auto inst =
            std::make_shared<zenkit::IEffectBase>();

        try
        {
            sc->vm->init_instance(
                inst,
                instSym
            );

            std::cout
                << "  [DEBUG] init_instance OK\n";
        }
        catch (const std::exception& e)
        {
            std::cerr
                << "  [ERROR] init_instance: "
                << e.what()
                << "\n";

            return;
        }

        std::cout
            << "\n"
            << "  ===== IEffectBase =====\n"

            << "  vis_name_s          = \""
            << inst->vis_name_s
            << "\"\n"

            << "  vis_size_s          = \""
            << inst->vis_size_s
            << "\"\n"

            << "  vis_alpha           = "
            << inst->vis_alpha
            << "\n"

            << "  em_trj_target_range = "
            << inst->em_trj_target_range
            << "\n"

            << "  =========================\n"
            << std::endl;

        return;
    }


    // ============================================================
    // INNY TYP
    // ============================================================

    std::cout
        << "\n"
        << "  ============================================\n"
        << "  NIEZNANY TYP RODZICA\n"
        << "  ============================================\n"

        << "  parent name  = "
        << parentName
        << "\n"

        << "  parent index = "
        << parentSym->index()
        << "\n"

        << "  parent type  = "
        << static_cast<int>(parentSym->type())
        << "\n"

        << "  parent count = "
        << parentSym->count()
        << "\n";


    // ============================================================
    // DLA NIEZNANEGO TYPU TEŻ WYPISUJEMY POLA
    // ============================================================

    std::cout
        << "\n"
        << "  ===== POLA KLASY =====\n";

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

            << " index="
            << field->index()

            << " type="
            << static_cast<int>(field->type())

            << " count="
            << field->count()

            << " name="
            << field->name()

            << " parent="
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