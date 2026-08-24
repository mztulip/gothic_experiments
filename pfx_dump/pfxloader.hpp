#pragma once

#include "PfxParams.hpp"
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

            auto pfx = std::make_shared<zenkit::IParticleEffect>();
            pfx->vis_tex_is_quadpoly = 1;

            try {
                sc->vm->init_instance(pfx, sym);
                outParams = extractParams(*pfx);
                outParams.originDatFile = sc->fileName;
                return true;
            } catch (...) {
                return false;
            }
        }
        return false;
    }
};