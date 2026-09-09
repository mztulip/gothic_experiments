#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstddef>

#include <epoxy/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <zenkit/ModelScript.hh>
#include <zenkit/ModelHierarchy.hh>
#include <zenkit/ModelAnimation.hh>
#include <zenkit/ModelMesh.hh>
#include <zenkit/Model.hh>
#include <zenkit/MultiResolutionMesh.hh>
#include <zenkit/Stream.hh>
#include <zenkit/Vfs.hh>

#include "structs.hpp"

namespace fs = std::filesystem;

inline std::string to_upper(std::string_view s)
{
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return r;
}

inline bool is_dir(const zenkit::VfsNode& n)
{
    return n.type() == zenkit::VfsNodeType::DIRECTORY;
}

inline const zenkit::VfsNode* find_vfs(
    const zenkit::VfsNode& n,
    const std::string& name)
{
    if (to_upper(n.name()) == name)
        return &n;

    if (!is_dir(n))
        return nullptr;

    for (const auto& child : n.children())
    {
        if (auto* found = find_vfs(child, name))
            return found;
    }

    return nullptr;
}

inline void mount_vfs(
    zenkit::Vfs& vfs,
    const fs::path& data_dir)
{
    for (const auto& entry : fs::directory_iterator(data_dir))
    {
        if (!entry.is_regular_file())
            continue;

        const auto ext = to_upper(entry.path().extension().string());

        if (ext != ".VDF" && ext != ".MDA" && ext != ".PAK")
            continue;

        try
        {
            vfs.mount_disk(entry.path(), zenkit::VfsOverwriteBehavior::ALL);
            std::cout << "[VFS] Zamontowano: " << entry.path().filename().string() << '\n';
        }
        catch (const std::exception& ex)
        {
            std::cerr << "[VFS WARNING] " << entry.path().filename().string() << ": " << ex.what() << '\n';
        }
    }
}

// Bezpieczne wypisywanie szczegółów MSB zgadzające się z interfejsem ZenKit
inline void dump_msb_details(const zenkit::ModelScript& msb)
{
    std::cout << "\n================ [ MSB PARAMS DUMP ] ================\n";
    std::cout << "Szkielet (Skeleton): " << msb.skeleton.name << '\n';

    std::cout << "\n--- Model Meshes (" << msb.meshes.size() << ") ---\n";
    for (size_t i = 0; i < msb.meshes.size(); ++i)
    {
        std::cout << "  [" << i << "] " << msb.meshes[i] << '\n';
    }

    std::cout << "\n--- Disabled Animations (" << msb.disabled_animations.size() << ") ---\n";
    for (const auto& disabled : msb.disabled_animations)
    {
        std::cout << "  - " << disabled << '\n';
    }

    std::cout << "\n--- Model Tags (" << msb.model_tags.size() << ") ---\n";
    for (size_t i = 0; i < msb.model_tags.size(); ++i)
    {
        std::cout << "  [" << i << "] Bone: " << msb.model_tags[i].bone << '\n';
    }

    std::cout << "\n--- Animations (" << msb.animations.size() << ") ---\n";
    for (size_t i = 0; i < msb.animations.size(); ++i)
    {
        const auto& anim = msb.animations[i];
        std::cout << "  [" << i << "] Name: " << anim.name
                  << " | Layer: " << anim.layer
                  << " | Next: " << anim.next
                  << " | Blend In: " << anim.blend_in
                  << " | Blend Out: " << anim.blend_out
                  << " | Flags: " << static_cast<uint32_t>(anim.flags)
                  << " | Model/Src: " << anim.model
                  << " | Direction: " << static_cast<int>(anim.direction)
                  << " | First Frame: " << anim.first_frame
                  << " | Last Frame: " << anim.last_frame
                  << " | Speed: " << anim.fps
                  << '\n';
    }

    std::cout << "\n--- Animation Aliases (" << msb.aliases.size() << ") ---\n";
    for (size_t i = 0; i < msb.aliases.size(); ++i)
    {
        const auto& alias = msb.aliases[i];
        std::cout << "  [" << i << "] Alias: " << alias.name 
                  << " -> Target: " << alias.alias 
                  << " | Layer: " << alias.layer 
                  << " | Direction: " << static_cast<int>(alias.direction) << '\n';
    }

    std::cout << "\n--- Animation Blends (" << msb.blends.size() << ") ---\n";
    for (size_t i = 0; i < msb.blends.size(); ++i)
    {
        const auto& blend = msb.blends[i];
        std::cout << "  [" << i << "] Blend: " << blend.name 
                  << " -> Next: " << blend.next 
                  << " | Blend In: " << blend.blend_in 
                  << " | Blend Out: " << blend.blend_out << '\n';
    }

    std::cout << "=====================================================\n\n";
}

struct LoadedCharacter
{
    zenkit::ModelScript script;
    zenkit::ModelHierarchy hierarchy;
    zenkit::ModelMesh mesh;
    std::vector<zenkit::ModelAnimation> animations;
    bool has_mesh{false};
    bool valid{false};
};

inline LoadedCharacter load_character_smart(
    const std::string& gothic_dir,
    const std::string& model_name)
{
    LoadedCharacter result{};
    zenkit::Vfs vfs;

    const auto data_dir = fs::path(gothic_dir) / "Data";
    if (!fs::exists(data_dir))
    {
        std::cerr << "[ERROR] Brak katalogu Data/: " << data_dir << '\n';
        return result;
    }

    mount_vfs(vfs, data_dir);

    std::string base_name = to_upper(fs::path(model_name).stem().string());

    // 1. MSB / MDS
    std::string msb_name = base_name + ".MSB";
    std::string mds_name = base_name + ".MDS";

    const zenkit::VfsNode* script_node = find_vfs(vfs.root(), msb_name);
    if (!script_node)
        script_node = find_vfs(vfs.root(), mds_name);

    if (!script_node)
    {
        std::cerr << "[ERROR] Nie znaleziono skryptu (.MSB/.MDS) dla: " << base_name << '\n';
        return result;
    }

    std::cout << "[INFO] Ładowanie skryptu: " << script_node->name() << '\n';
    try
    {
        auto reader = script_node->open_read();
        result.script.load(reader.get());
        
        dump_msb_details(result.script);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[ERROR] Błąd MSB/MDS: " << ex.what() << '\n';
        return result;
    }

    // 2. MDH
    std::string mdh_name = base_name + ".MDH";
    const zenkit::VfsNode* mdh_node = find_vfs(vfs.root(), mdh_name);

    if (mdh_node)
    {
        std::cout << "[INFO] Ładowanie szkieletu: " << mdh_node->name() << '\n';
        try
        {
            auto reader = mdh_node->open_read();
            result.hierarchy.load(reader.get());
            std::cout << "[INFO] Liczba kości w szkieledzie: " << result.hierarchy.nodes.size() << '\n';
        }
        catch (const std::exception& ex)
        {
            std::cerr << "[ERROR] Błąd MDH: " << ex.what() << '\n';
        }
    }

    // 3. MDM lub .3DS
    std::vector<std::string> mdm_candidates;

    // Przetwarzamy wpisy siatek z pliku MSB/MDS (np. KRO_BODY.ASC -> KRO_BODY.MDM / KRO_BODY.3DS)
    for (const auto& mesh_name : result.script.meshes)
    {
        std::string upper_mesh = to_upper(mesh_name);
        fs::path p(upper_mesh);
        std::string stem = p.stem().string(); // wyciąga "KRO_BODY" z "KRO_BODY.ASC"

        mdm_candidates.push_back(stem + ".MDM");
        mdm_candidates.push_back(stem + ".3DS");
    }

    // Zapasowe wzorce bazujące na nazwie wywołania (np. ALLIGATOR)
    mdm_candidates.push_back(base_name + "_BODY.MDM");
    mdm_candidates.push_back(base_name + ".MDM");
    mdm_candidates.push_back(base_name + "_BODY.3DS");
    mdm_candidates.push_back(base_name + ".3DS");

    const zenkit::VfsNode* mdm_node = nullptr;
    for (const auto& candidate : mdm_candidates)
    {
        mdm_node = find_vfs(vfs.root(), candidate);
        if (mdm_node) break;
    }

    if (mdm_node)
    {
        std::cout << "[INFO] Ładowanie siatki modelu: " << mdm_node->name() << '\n';
        try
        {
            auto reader = mdm_node->open_read();
            std::string ext = to_upper(fs::path(mdm_node->name()).extension().string());

            if (ext == ".MDM")
            {
                zenkit::ModelMesh mesh;
                mesh.load(reader.get());

                std::cout << "[DEBUG] ModelMesh meshes: "
                        << mesh.meshes.size() << '\n';

                result.mesh = std::move(mesh);
                result.has_mesh = true;

                std::cout << "[INFO] Załadowano geometrię z pliku .MDM\n";
            }
            else if (ext == ".3DS")
            {
                zenkit::MultiResolutionMesh mrm;
                mrm.load(reader.get());

                zenkit::SoftSkinMesh skin;
                skin.mesh = std::move(mrm);
                result.mesh.meshes.push_back(std::move(skin));
                result.has_mesh = true;
                std::cout << "[INFO] Załadowano geometrię z pliku .3DS (MultiResolutionMesh) i dołączono do SoftSkinMesh\n";
            }
        }
        catch (const std::exception& ex)
        {
            std::cerr << "[ERROR] Błąd ładowania siatki: " << ex.what() << '\n';
        }
    }
    else
    {
        std::cerr << "[WARNING] Nie odnaleziono pliku siatki (.MDM / .3DS) dla: " << base_name << '\n';
    }

    // 4. MAN
    if (!result.script.animations.empty())
    {
        const auto& anim_def = result.script.animations[0];
        std::string anim_name_upper = to_upper(anim_def.name);
        std::string anim_model_upper = to_upper(anim_def.model);

        std::vector<std::string> man_candidates = {
            base_name + "_" + anim_name_upper + ".MAN",
            base_name + "-" + anim_name_upper + ".MAN",
            anim_model_upper,
            anim_name_upper + ".MAN"
        };

        const zenkit::VfsNode* man_node = nullptr;
        for (const auto& candidate : man_candidates)
        {
            if (candidate.empty()) continue;
            man_node = find_vfs(vfs.root(), candidate);
            if (man_node) break;
        }

        if (man_node)
        {
            std::cout << "[INFO] Ładowanie animacji: " << man_node->name() << '\n';
            try
            {
                auto reader = man_node->open_read();
                zenkit::ModelAnimation anim;
                anim.load(reader.get());
                result.animations.push_back(std::move(anim));
                std::cout << "[INFO] Załadowano animację: " << anim_def.name
                          << " (Klatek: " << result.animations.back().frame_count << ")\n";
            }
            catch (const std::exception& ex)
            {
                std::cerr << "[ERROR] Błąd MAN: " << ex.what() << '\n';
            }
        }
        else
        {
            std::cerr << "[WARNING] Nie znaleziono pliku .MAN dla animacji: " << anim_def.name << '\n';
        }
    }

    result.valid = true;
    return result;
}