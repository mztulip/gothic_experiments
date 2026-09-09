#pragma once

#include <zenkit/Vfs.hh>
#include <filesystem>
#include <string>
#include <algorithm>
#include <cstdio>

static zenkit::Vfs& gothicVfs(const std::string& gothicDir)
{
    static zenkit::Vfs vfs;
    static bool mounted = false;

    if (!mounted)
    {
        mounted = true;
        namespace fs = std::filesystem;
        fs::path dataDir = fs::path(gothicDir) / "Data";

        if (!fs::exists(dataDir))
        {
            fprintf(stderr, "[VFS] Brak katalogu Data: %s\n", dataDir.string().c_str());
            return vfs;
        }

        int count = 0;
        for (const auto& entry : fs::directory_iterator(dataDir))
        {
            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                            [](unsigned char c){ return std::tolower(c); });

            if (ext != ".vdf") continue;

            try
            {
                vfs.mount_disk(entry.path(), zenkit::VfsOverwriteBehavior::OLDER);
                ++count;
            }
            catch (const std::exception& e)
            {
                fprintf(stderr, "[VFS] Nie udalo sie zamontowac %s: %s\n",
                        entry.path().string().c_str(), e.what());
            }
        }
        printf("[VFS] Zamontowano %d archiwow VDF z %s\n", count, dataDir.string().c_str());
        fflush(stdout);
    }

    return vfs;
}

// Rekurencyjnie zbiera nazwy plikow o danym rozszerzeniu (np. ".MRM") z VFS.
// TODO(verify): zaklada API VfsNode z metodami .type(), .name(), .children().
// Jesli Twoja wersja zenkit ma inne nazwy, popraw ta jedna funkcje.
static void collectVfsFilesByExt(const zenkit::VfsNode& node, const std::string& extLower,
                                  std::vector<std::string>& out)
{
    if (node.type() == zenkit::VfsNodeType::DIRECTORY)
    {
        for (const auto& child : node.children())
            collectVfsFilesByExt(child, extLower, out);
        return;
    }

    std::string name = node.name();
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                    [](unsigned char c){ return std::tolower(c); });

    if (nameLower.size() >= extLower.size() &&
        nameLower.compare(nameLower.size() - extLower.size(), extLower.size(), extLower) == 0)
    {
        out.push_back(name);
    }
}

static std::vector<std::string> listVfsFilesByExt(zenkit::Vfs& vfs, const std::string& extLower)
{
    std::vector<std::string> out;
    collectVfsFilesByExt(vfs.root(), extLower, out);
    return out;
}

