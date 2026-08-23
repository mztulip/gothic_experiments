#ifndef TEXTURE_LOADER_H
#define TEXTURE_LOADER_H

#include <epoxy/gl.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <string>
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

struct Texture2D
{
    GLuint id = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::string path;
    bool valid = false;

    void free()
    {
        if (id != 0)
        {
            glDeleteTextures(1, &id);
            id = 0;
        }
        valid = false;
    }
};

class TextureLoader
{
public:
    static std::string resolveGothicTexturePath(const std::string& requestedFilename, const std::string& gothicDir)
    {
        fs::path gothicTexDir = fs::path(gothicDir) / "_Work" / "Data" / "Textures";
        if (!fs::exists(gothicTexDir))
        {
            gothicTexDir = gothicDir;
        }

        std::string targetLower = requestedFilename;
        std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);

        std::string targetTexLower = fs::path(targetLower).replace_extension(".tex").string();

        try
        {
            for (const auto& entry : fs::recursive_directory_iterator(gothicTexDir))
            {
                if (entry.is_regular_file())
                {
                    std::string currentFile = entry.path().filename().string();
                    std::transform(currentFile.begin(), currentFile.end(), currentFile.begin(), ::tolower);

                    if (currentFile == targetLower || currentFile == targetTexLower)
                    {
                        return entry.path().string();
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "[TEXTURE ERROR] Blad podczas przeszukiwania katalogu: " << e.what() << std::endl;
        }

        return "";
    }

    static Texture2D loadFromFile(const std::string& filepath, bool flipVertically = false)
    {
        Texture2D tex;
        tex.path = filepath;

        std::cout << "[TEXTURE LOG] Proba wczytania pliku: " << filepath << std::endl;

        int w, h, comp;
        stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);
        unsigned char* data = stbi_load(filepath.c_str(), &w, &h, &comp, 4);

        if (data)
        {
            glGenTextures(1, &tex.id);
            glBindTexture(GL_TEXTURE_2D, tex.id);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            stbi_image_free(data);

            tex.width = w;
            tex.height = h;
            tex.channels = comp;
            tex.valid = true;

            std::cout << "  -> SUKCES: Zaladowano teksture (" << w << "x" << h << " px)" << std::endl;
        }
        else
        {
            tex.valid = false;
            std::cerr << "  -> BLAD STB_IMAGE: Nie udalo sie zdekodowac pliku!" << std::endl;
        }

        return tex;
    }
};

#endif // TEXTURE_LOADER_H