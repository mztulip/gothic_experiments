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
    static bool tryLoadPfx(const std::string& name, const std::vector<std::shared_ptr<LoadedScript>>& scripts, PfxParams& out)
    {
        for (const auto& sc : scripts) {
            if (!sc || !sc->vm) continue;

            auto* sym = sc->vm->find_symbol_by_name(name);
            if (!sym) continue;

            // Sprawdzamy czy symbol dziedziczy po C_PARTICLEFX
            auto* parentSym = sc->vm->find_symbol_by_index(sym->parent());
            if (parentSym && parentSym->name() != "C_PARTICLEFX") {
                continue; // To nie jest PFX, pomijamy
            }

            try {
                auto pfx = std::make_shared<zenkit::IParticleEffect>();
                pfx->vis_tex_is_quadpoly = 1;

                sc->vm->init_instance(pfx, sym);
                out = extractPfxParams(*pfx);
                out.originDatFile = sc->fileName;
                return true;
            } catch (...) {
                continue;
            }
        }
        return false;
    }
};