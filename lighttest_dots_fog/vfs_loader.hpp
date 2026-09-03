#pragma once

#include <zenkit/Vfs.hh>
#include <filesystem>
#include <string>
#include <cstdio>

// Zwraca zamontowany VFS (lazy init, budowany raz)
static zenkit::Vfs& gothicVfs(const std::string& gothicDir)
{
    static zenkit::Vfs vfs;
    static bool mounted = false;

    printf("[VFS DEBUG] wejscie do gothicVfs, gothicDir=%s\n", gothicDir.c_str());
    fflush(stdout);

    if (!mounted)
    {
        mounted = true;

        namespace fs = std::filesystem;
        fs::path dataDir = fs::path(gothicDir) / "Data";

        printf("[VFS DEBUG] dataDir=%s exists=%d\n", dataDir.string().c_str(), fs::exists(dataDir));
        fflush(stdout);

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

            printf("[VFS DEBUG] montuje: %s\n", entry.path().string().c_str());
            fflush(stdout);

            try
            {
                vfs.mount_disk(entry.path(), zenkit::VfsOverwriteBehavior::OLDER);
                printf("[VFS DEBUG]   OK\n");
                fflush(stdout);
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