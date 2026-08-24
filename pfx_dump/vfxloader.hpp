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

    // WAŻNE:
    // instSym pochodzi z VM, więc rodzica również szukamy przez VM.
    auto* parentSym =
        sc->vm->find_symbol_by_index(instSym->parent());

    std::string parentName =
        parentSym ? parentSym->name() : "Nieznana";

    std::cout
        << "=== Pola w "
        << instSym->name()
        << " (Klasa: "
        << parentName
        << ") ==="
        << std::endl;

std::cout << "  [DEBUG] symbol index = "
          << instSym->index()
          << "\n";

std::cout << "  [DEBUG] symbol type = "
          << static_cast<int>(instSym->type())
          << "\n";

std::cout << "  [DEBUG] symbol parent = "
          << instSym->parent()
          << "\n";

std::cout << "  [DEBUG] symbol count = "
          << instSym->count()
          << "\n";

          
    // ============================================================
    // C_PARTICLEFXEMITKEY
    // ============================================================

    if (parentName == "C_PARTICLEFXEMITKEY")
    {
        // auto inst =
        //     std::make_shared<zenkit::IParticleEffectEmitKey>();

        // sc->vm->init_instance(inst, instSym);
        auto inst =
        std::make_shared<zenkit::IParticleEffectEmitKey>();

        std::cout << "  [DEBUG] IPFX object utworzony" << std::endl;

        // sc->vm->init_instance(inst, instSym);

        std::cout << "  [DEBUG] init_instance pominięte" << std::endl;


        // std::cout
        //     << "  vis_name_s: \""
        //     << inst->vis_name_s
        //     << "\"\n"

        //     << "  pfx_pps_value: "
        //     << inst->pfx_pps_value
        //     << "\n"

        //     << "  pfx_pps_is_looping_chg: "
        //     << inst->pfx_pps_is_looping_chg
        //     << "\n"

        //     << "  pfx_sc_time: "
        //     << inst->pfx_sc_time
        //     << "\n"

        //     << "  pfx_shp_is_volume_chg: "
        //     << inst->pfx_shp_is_volume_chg
        //     << "\n"

        //     << "  pfx_shp_scale_fps: "
        //     << inst->pfx_shp_scale_fps
        //     << "\n"

        //     << "  pfx_pps_is_smooth_chg: "
        //     << inst->pfx_pps_is_smooth_chg
        //     << "\n";

        return;
    }


    // ============================================================
    // C_XIVISUALFX / C_XIVISUALFX_D
    // ============================================================

    if (parentName == "C_XIVISUALFX" ||
        parentName == "C_XIVISUALFX_D")
    {
        auto inst =
            std::make_shared<zenkit::IEffectBase>();

        sc->vm->init_instance(inst, instSym);

        std::cout
            << "  vis_name_s: \""
            << inst->vis_name_s
            << "\"\n"

            << "  vis_size_s: \""
            << inst->vis_size_s
            << "\"\n"

            << "  user_string[0]: \""
            << inst->user_string[0]
            << "\"\n"

            << "  em_fx_create_s: \""
            << inst->em_fx_create_s
            << "\"\n"

            << "  em_fx_invest_origin_s: \""
            << inst->em_fx_invest_origin_s
            << "\"\n"

            << "  em_trj_target_range: "
            << inst->em_trj_target_range
            << "\n";

        return;
    }

    if (parentName == "CFX_BASE_PROTO")
    {
        std::cout << "  [DEBUG] Próba utworzenia IEffectBase...\n";

        auto inst = std::make_shared<zenkit::IEffectBase>();

        std::cout << "  [DEBUG] IEffectBase utworzony\n";

        sc->vm->init_instance(inst, instSym);

    std::cout
        << "\n"
        << "  ===== IEffectBase =====\n"
        << "  vis_name_s              = \""
        << inst->vis_name_s << "\"\n"

        << "  vis_size_s              = \""
        << inst->vis_size_s << "\"\n"

        << "  user_string[0]          = \""
        << inst->user_string[0] << "\"\n"

        << "  user_string[1]          = \""
        << inst->user_string[1] << "\"\n"

        << "  user_string[2]          = \""
        << inst->user_string[2] << "\"\n"

        << "  user_string[3]          = \""
        << inst->user_string[3] << "\"\n"

        << "  em_fx_create_s          = \""
        << inst->em_fx_create_s << "\"\n"

        << "  em_fx_invest_origin_s   = \""
        << inst->em_fx_invest_origin_s << "\"\n"

        << "  em_trj_target_range     = "
        << inst->em_trj_target_range << "\n"

        << "  =========================\n"
        << std::endl;


        return;
    }


    // ============================================================
    // FALLBACK
    // ============================================================
    //
    // Na razie NIE robimy init_opaque_instance().
    // Najpierw chcemy mieć pewność, że VFX/PFX działają.
    //

    std::cout
    << "  [VFX Loader] Analizuję członków klasy: "
    << parentName
    << "\n";

    if (!parentSym)
    {
        std::cout << "  [DEBUG] Brak parentSym!\n";
        return;
    }

    std::cout
    << "  [DEBUG] parent index = "
    << parentSym->index()
    << "\n"
    << "  [DEBUG] parent name = "
    << parentSym->name()
    << "\n"
    << "  [DEBUG] parent type = "
    << static_cast<int>(parentSym->type())
    << "\n"
    << "  [DEBUG] parent count = "
    << parentSym->count()
    << "\n"
    << "  [DEBUG] parent parent-index = "
    << parentSym->parent()
    << "\n";


    auto* grandParent =
    sc->script.find_symbol_by_index(parentSym->parent());

    if (grandParent)
    {
        std::cout
            << "  [DEBUG] grandParent name = "
            << grandParent->name()
            << "\n"
            << "  [DEBUG] grandParent type = "
            << static_cast<int>(grandParent->type())
            << "\n"
            << "  [DEBUG] grandParent index = "
            << grandParent->index()
            << "\n";
    }
    else
    {
        std::cout << "  [DEBUG] grandParent = nullptr\n";
    }


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
         std::cout<<"Vfx loading failed"<<std::endl;
        return false;
    }
};