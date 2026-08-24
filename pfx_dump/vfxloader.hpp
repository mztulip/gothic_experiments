#pragma once

#include "pfxloader.hpp"
#include <zenkit/DaedalusVm.hh>
#include <zenkit/addon/daedalus.hh>
#include <memory>
#include <string>
#include <vector>

class VfxLoader {
public:
    static bool tryLoadVfx(const std::string& name, 
                           const std::vector<std::shared_ptr<LoadedScript>>& scripts, 
                           PfxParams& outParams) 
    {
        for (const auto& sc : scripts) {
            if (!sc || !sc->vm) continue;

            auto* sym = sc->vm->find_symbol_by_name(name);
            if (!sym) continue;

            // Sprawdzamy czy symbol to faktycznie C_XIVISUALFX / C_PARTICLEFX_EMITTER
            auto* parentSym = sc->vm->find_symbol_by_index(sym->parent());
            if (parentSym && parentSym->name() != "C_XIVISUALFX" && parentSym->name() != "C_PARTICLEFX_EMITTER") {
                continue;
            }

            try {
                // Prawidłowa klasa w ZenKicie to IEffectBase
                auto vfx = std::make_shared<zenkit::IEffectBase>();
                sc->vm->init_instance(vfx, sym);

                std::string childPfxName = vfx->em_fx_create_s;

                // Jeśli VFX zawiera podpięty PFX (np. em_fx_create_s), ładujemy go
                if (!childPfxName.empty() && childPfxName != name) {
                    if (PfxLoader::tryLoadPfx(childPfxName, scripts, outParams)) {
                        outParams.originDatFile = sc->fileName + " -> " + childPfxName;
                        return true;
                    }
                }

                // Awaryjnie wyciągamy teksturę/wizualia bezpośrednio z VFX
                outParams = PfxParams();
                outParams.visName = vfx->vis_name_s;
                outParams.ppsValue = 5.0f;
                outParams.originDatFile = sc->fileName;
                return true;
            } catch (...) {
                continue;
            }
        }
        return false;
    }
};