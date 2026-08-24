#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <algorithm>

#include <zenkit/DaedalusScript.hh>
#include <zenkit/DaedalusVm.hh>
#include <zenkit/Stream.hh>
#include <zenkit/addon/daedalus.hh>

#include "LoadedScript.hpp"
#include "PfxParams.hpp"
#include "StringUtils.hpp"
#include "pfxloader.hpp"
#include "vfxloader.hpp"

namespace fs = std::filesystem;

class ParticleLibrary {
public:
    const auto& getScripts() const { return scripts; }

    bool loadFile(const std::string& datPath) {
        std::string filename = fs::path(datPath).filename().string();
        auto loaded = std::make_unique<LoadedScript>();
        loaded->fileName = filename;

        try {
            auto reader = zenkit::Read::from(datPath);
            loaded->script.load(reader.get());
        } catch (const std::exception& e) {
            fprintf(stderr, "[Library] Nie udalo sie wczytac %s: %s\n", datPath.c_str(), e.what());
            return false;
        }

        try { zenkit::IParticleEffect::register_(loaded->script); } catch (...) {}

        std::vector<uint32_t> parentIndices;
        const char* classNames[] = { "C_PARTICLEFX", "C_PARTICLEFXEMITHP", "C_XIVISUALFX", "CFX" };
        for (const char* className : classNames) {
            auto* cls = loaded->script.find_symbol_by_name(className);
            if (cls != nullptr) parentIndices.push_back(cls->index());
        }

        for (auto& sym : loaded->script.symbols()) {
            if (sym.type() == zenkit::DaedalusDataType::INSTANCE || sym.type() == zenkit::DaedalusDataType::PROTOTYPE) {
                bool isPfx = false;
                for (uint32_t pIdx : parentIndices) {
                    if (sym.parent() == pIdx) { isPfx = true; break; }
                }

                if (isPfx || containsIgnoreCase(sym.name(), "PFX") || containsIgnoreCase(sym.name(), "POTION") || containsIgnoreCase(sym.name(), "SPELL")) {
                    if (effectOriginMap.find(sym.name()) == effectOriginMap.end()) {
                        effectNames.push_back(sym.name());
                        effectOriginMap[sym.name()] = filename;
                    }
                }
            }
        }

        // W ParticleLibrary::loadFile:
        loaded->vm = std::make_shared<zenkit::DaedalusVm>(
            std::move(loaded->script), 
            zenkit::DaedalusVmExecutionFlag::ALLOW_NULL_INSTANCE_ACCESS
        );
        scripts.push_back(std::move(loaded));
        std::sort(effectNames.begin(), effectNames.end());
        return true;
    }

    const std::vector<std::string>& names() const { return effectNames; }

    std::string getOriginFile(const std::string& name) const {
        auto it = effectOriginMap.find(name);
        return it != effectOriginMap.end() ? it->second : "Nieznany";
    }

    bool getParams(const std::string& name, PfxParams& out) const {
        if (PfxLoader::tryLoadPfx(name, scripts, out)) return true;
        return VfxLoader::tryLoadVfx(name, scripts, out);
    }

    static std::vector<std::string> discoverDatFiles(int argc, char** argv) {
        std::vector<std::string> result;
        std::string baseDir;

        if (argc >= 2) {
            baseDir = fs::path(argv[1]).parent_path().string();
            result.push_back(argv[1]);
        } else {
            const char* env = std::getenv("GOTHIC2_DIR");
            if (env && env[0] != '\0') baseDir = std::string(env) + "/_Work/Data/Scripts/_compiled/";
        }

        if (!baseDir.empty() && fs::exists(baseDir)) {
            std::string pfx = baseDir + "/PARTICLEFX.DAT";
            std::string vfx = baseDir + "/VISUALFX.DAT";
            if (fs::exists(pfx) && std::find(result.begin(), result.end(), pfx) == result.end()) result.push_back(pfx);
            if (fs::exists(vfx) && std::find(result.begin(), result.end(), vfx) == result.end()) result.push_back(vfx);
        }
        return result;
    }

    static std::string resolveGothicDir(const std::vector<std::string>& datPaths) {
        const char* envDir = std::getenv("GOTHIC2_DIR");
        if (envDir && envDir[0] != '\0') return std::string(envDir);
        if (!datPaths.empty()) {
            fs::path p(datPaths[0]);
            while (p.has_parent_path() && p.filename() != "_Work") p = p.parent_path();
            return (p.filename() == "_Work") ? p.parent_path().string() : fs::path(datPaths[0]).parent_path().string();
        }
        return "";
    }

private:
    std::vector<std::shared_ptr<LoadedScript>> scripts;
    std::vector<std::string> effectNames;
    std::unordered_map<std::string, std::string> effectOriginMap;
};