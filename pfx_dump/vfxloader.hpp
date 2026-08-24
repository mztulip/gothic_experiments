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

            auto* parentSym = sc->script.find_symbol_by_index(sym->parent());
            std::string parentName = parentSym ? parentSym->name() : "";

            if (containsIgnoreCase(parentName, "C_XIVISUALFX") || 
                containsIgnoreCase(parentName, "CFX")) 
            {
                try {
                    // W ZenKit używamy zenkit::IEffectBase
                    auto vfx = std::make_shared<zenkit::IEffectBase>();
                    sc->vm->init_instance(vfx, sym);

                    std::string childPfxName = vfx->em_fx_create_s;

                    // Jeśli VFX odwołuje się do cząsteczki PFX
                    if (!childPfxName.empty() && !containsIgnoreCase(childPfxName, name)) {
                        if (PfxLoader::tryLoadPfx(childPfxName, scripts, outParams)) {
                            outParams.originDatFile = sc->fileName + " -> " + childPfxName;
                            return true;
                        }
                    }

                    // Jeśli VFX wskazuje bezpośrednio na teksturę / mesh
                    outParams = PfxParams();
                    outParams.visName = vfx->vis_name_s;
                    outParams.ppsValue = 5.0f;
                    outParams.originDatFile = sc->fileName + " (VisualFX Mesh)";
                    return true;
                } catch (...) {
                    return false;
                }
            }
        }
        return false;
    }
};