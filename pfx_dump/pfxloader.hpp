#pragma once

#include "PfxParams.hpp"
#include "StringUtils.hpp"
#include "LoadedScript.hpp"
#include <zenkit/DaedalusVm.hh>
#include <zenkit/addon/daedalus.hh>
#include <memory>
#include <string>
#include <vector>

class PfxLoader {
public:
    static bool tryLoadPfx(const std::string& name, 
                           const std::vector<std::shared_ptr<LoadedScript>>& scripts, 
                           PfxParams& outParams) 
    {
        for (const auto& sc : scripts) {
            if (!sc || !sc->vm) continue;

            auto* sym = sc->vm->find_symbol_by_name(name);
            if (!sym) continue;

            auto* parentSym = sc->script.find_symbol_by_index(sym->parent());
            std::string parentName = parentSym ? parentSym->name() : "";

            if (containsIgnoreCase(parentName, "C_PARTICLEFX") || 
                containsIgnoreCase(parentName, "C_PARTICLEFXEMITHP")) 
            {
                try {
                    auto pfx = std::make_shared<zenkit::IParticleEffect>();
                    sc->vm->init_instance(pfx, sym);

                    outParams = extractParams(*pfx);

                    // Jeśli visName jest puste po extractParams, pobieramy bezpośrednio z pola vis_name_s
                    if (outParams.visName.empty()) {
                        outParams.visName = pfx->vis_name_s;
                    }

                    outParams.originDatFile = sc->fileName;
                    return true;
                } catch (...) {
                    return false;
                }
            }
        }
        return false;
    }
};