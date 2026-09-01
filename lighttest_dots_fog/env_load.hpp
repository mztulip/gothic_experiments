#pragma once

#include <string>

static std::string getGothicDir()
{
    const char* env = std::getenv("GOTHIC2_DIR");

    if (!env)
    {
        printf("GOTHIC2_DIR: NIE USTAWIONA\n");
        return {};
    }

    printf("GOTHIC2_DIR RAW: %s\n", env);

    std::string path(env);

    // Usuwamy backslashe używane przez shell do escapowania spacji
    path.erase(
        std::remove(path.begin(), path.end(), '\\'),
        path.end()
    );

    printf("GOTHIC2_DIR: %s\n", path.c_str());

    return path;
}


static std::string findMeshFile(
    const std::string& gothicDir,
    const std::string& visualName)
{
    if (visualName.empty())
        return {};

    namespace fs = std::filesystem;

    fs::path meshesDir =
    fs::path(gothicDir) / "_Work" / "Data" / "Meshes";

    printf("SZUKAM MESHY W: %s\n", meshesDir.string().c_str());

    if (!fs::exists(meshesDir))
    {
        printf("KATALOG MESHES NIE ISTNIEJE!\n");
        return {};
    }

    printf("KATALOG MESHES ISTNIEJE.\n");


    std::string wanted = visualName;

    std::transform(
        wanted.begin(),
        wanted.end(),
        wanted.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });

    try
    {
        for (const auto& entry :
             fs::recursive_directory_iterator(meshesDir))
        {
            if (!entry.is_regular_file())
                continue;

            std::string filename =
                entry.path().filename().string();

            std::transform(
                filename.begin(),
                filename.end(),
                filename.begin(),
                [](unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });

            if (filename == wanted)
                return entry.path().string();
        }
    }
    catch (const std::exception& e)
    {
        fprintf(stderr,
                "Blad podczas szukania mesha: %s\n",
                e.what());
    }

    return {};
}
