#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <cstdio>
#include <cstdlib>


static std::string getGothicDir()
{
    const char* env = std::getenv("GOTHIC2_DIR");

    if (!env)
    {
        // printf("GOTHIC2_DIR: NIE USTAWIONA\n");
        return {};
    }

    // printf("GOTHIC2_DIR RAW: %s\n", env);

    std::string path(env);

    // Usuwamy backslashe używane przez shell do escapowania spacji
    path.erase(
        std::remove(path.begin(), path.end(), '\\'),
        path.end()
    );

    // printf("GOTHIC2_DIR: %s\n", path.c_str());

    return path;
}

static std::string toLowerStr(const std::string& s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Indeks budowany RAZ, przy pierwszym wywolaniu (lazy init).
// klucz = nazwa pliku (lowercase), wartosc = pelna sciezka
static std::unordered_map<std::string, std::string>& meshFileIndex(const std::string& gothicDir)
{
    static std::unordered_map<std::string, std::string> index;
    static bool built = false;

    if (!built)
    {
        built = true;

        namespace fs = std::filesystem;
        fs::path meshesDir = fs::path(gothicDir) / "_Work" / "Data" / "Meshes";

        if (fs::exists(meshesDir))
        {
            try
            {
                for (const auto& entry : fs::recursive_directory_iterator(meshesDir))
                {
                    if (!entry.is_regular_file())
                        continue;

                    std::string filename = toLowerStr(entry.path().filename().string());
                    index[filename] = entry.path().string();
                }

                printf("[MESH INDEX] Zindeksowano %zu plikow z %s\n",
                       index.size(), meshesDir.string().c_str());
            }
            catch (const std::exception& e)
            {
                fprintf(stderr, "Blad podczas indeksowania meshy: %s\n", e.what());
            }
        }
    }

    return index;
}

static std::string findMeshFile(
    const std::string& gothicDir,
    const std::string& visualName)
{
    if (visualName.empty())
        return {};

    auto& index = meshFileIndex(gothicDir);

    std::string wanted = toLowerStr(visualName);

    auto it = index.find(wanted);
    if (it != index.end())
        return it->second;

    return {};
}