#pragma once

#include <epoxy/gl.h>

#include <string>
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <algorithm>
#include <cctype>

#include <vector>
#include <cctype>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace fs = std::filesystem;

struct Texture2D {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    std::string path;
    bool valid = false;

    void free() {
        if (id != 0) {
            glDeleteTextures(1, &id);
            id = 0;
        }
        valid = false;
    }
};

enum class TextureSource {
    Both,        // mydata z pierwszenstwem, fallback do Gothica (domyslne, jak dotychczas)
    MyDataOnly,  // tylko katalog mydata
    GothicOnly   // tylko katalog Gothica, mydata pomijane calkowicie
};

class TextureCache {
private:
    std::unordered_map<std::string, std::string> fileIndex;
    bool indexed = false;

public:
    static std::string normalizeTextureName(std::string name)
    {
        // Gothic używa '\' jako separatora, Linux '/'
        std::replace(name.begin(), name.end(), '\\', '/');

        // lowercase
        std::transform(
            name.begin(),
            name.end(),
            name.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            }
        );

        // Usuwamy ewentualne ./ na początku
        while (name.rfind("./", 0) == 0)
            name.erase(0, 2);

        // Usuwamy prefiks textures/
        if (name.rfind("textures/", 0) == 0)
            name.erase(0, 9);

        return name;
    }

    // Indeksuje cały katalog tekstur RAZ na starcie (szybkie wyszukiwanie)
   void indexDirectory(const std::string& gothicDir,  TextureSource source = TextureSource::Both)
{
    fs::path customTexDir =
        fs::path("mydata") / "_Work" / "Data" / "Textures";

    fs::path gothicTexDir =
        fs::path(gothicDir) / "_Work" / "Data" / "Textures";

    // Fallback - jeżeli podany gothicDir jest już katalogiem Textures
    if (!fs::exists(gothicTexDir))
    {
        gothicTexDir = fs::path(gothicDir);
    }

    fileIndex.clear();

    try
    {
        // =========================================================
        // FUNKCJA POMOCNICZA
        // =========================================================
        auto indexTextureDirectory =
            [&](const fs::path& textureDir, bool overwrite)
        {
            if (!fs::exists(textureDir))
            {
                std::cout
                    << "[TEXTURE] Katalog nie istnieje: "
                    << textureDir
                    << std::endl;

                return;
            }

            std::cout
                << "[TEXTURE] Indeksowanie: "
                << textureDir
                << std::endl;

            for (const auto& entry :
                fs::recursive_directory_iterator(textureDir))
            {
                if (!entry.is_regular_file())
                    continue;

                fs::path absolutePath = entry.path();

                // -------------------------------------------------
                // 1. Sama nazwa pliku
                // np. wall.tga
                // -------------------------------------------------
                std::string filename =
                    normalizeTextureName(
                        absolutePath.filename().string()
                    );

                if (overwrite)
                {
                    fileIndex[filename] = absolutePath.string();
                }
                else
                {
                    fileIndex.emplace(
                        filename,
                        absolutePath.string()
                    );
                }

                // -------------------------------------------------
                // 2. Ścieżka względna
                // np. world/castle/wall.tga
                // -------------------------------------------------
                std::error_code ec;

                fs::path relativePath =
                    fs::relative(
                        absolutePath,
                        textureDir,
                        ec
                    );

                if (!ec)
                {
                    std::string relativeName =
                        normalizeTextureName(
                            relativePath.generic_string()
                        );

                    if (overwrite)
                    {
                        fileIndex[relativeName] =
                            absolutePath.string();
                    }
                    else
                    {
                        fileIndex.emplace(
                            relativeName,
                            absolutePath.string()
                        );
                    }
                }
            }
        };


        switch (source)
        {
            case TextureSource::Both:
                // CUSTOM ma pierwszenstwo, potem fallback do Gothica
                indexTextureDirectory(customTexDir, true);
                indexTextureDirectory(gothicTexDir, false);
                break;

            case TextureSource::MyDataOnly:
                indexTextureDirectory(customTexDir, true);
                // std::cout << "[TEXTURE] Tryb --mydata-only: katalog Gothica pominiety." << std::endl;
                break;

            case TextureSource::GothicOnly:
                indexTextureDirectory(gothicTexDir, true);
                // std::cout << "[TEXTURE] Tryb --gothic-only: katalog mydata pominiety." << std::endl;
                break;
        }

        indexed = true;

        std::cout
            << "[TEXTURE LOG] Zindeksowano "
            << fileIndex.size()
            << " nazw/ścieżek tekstur."
            << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "[TEXTURE ERROR] Błąd indeksowania: "
            << e.what()
            << std::endl;
    }
}

    std::string resolvePath(const std::string& requestedFilename)
    {
        std::string normalized =
            normalizeTextureName(requestedFilename);

        if (normalized.empty())
            return "";

        // ---------------------------------------------------------
        // Najpierw dokładnie to, o co poprosił materiał
        // ---------------------------------------------------------
        auto it = fileIndex.find(normalized);

        if (it != fileIndex.end())
            return it->second;

        // ---------------------------------------------------------
        // Jeżeli materiał podał np. .tga/.jpg/.png,
        // spróbuj odpowiadającego .tex
        // ---------------------------------------------------------
        fs::path p(normalized);

        std::string ext = p.extension().string();

        if (ext == ".tga" ||
            ext == ".jpg" ||
            ext == ".jpeg" ||
            ext == ".png")
        {
            std::string texName =
                p.replace_extension(".tex").generic_string();

            it = fileIndex.find(texName);

            if (it != fileIndex.end())
                return it->second;
        }

        // ---------------------------------------------------------
        // Ostatecznie spróbuj samego basename
        // ---------------------------------------------------------
        std::string basename =
            normalizeTextureName(
                fs::path(normalized).filename().string()
            );

        it = fileIndex.find(basename);

        if (it != fileIndex.end())
            return it->second;

        // ---------------------------------------------------------
        // I basename -> .tex
        // ---------------------------------------------------------
        fs::path basePath(basename);

        std::string baseExt =
            basePath.extension().string();

        if (baseExt == ".tga" ||
            baseExt == ".jpg" ||
            baseExt == ".jpeg" ||
            baseExt == ".png")
        {
            std::string texName =
                basePath.replace_extension(".tex").string();

            it = fileIndex.find(texName);

            if (it != fileIndex.end())
                return it->second;
        }

        return "";
    }


    Texture2D loadTexture(const std::string& requestedFilename)
    {
        Texture2D tex;

        std::string resolvedPath = resolvePath(requestedFilename);

        if (resolvedPath.empty())
        {
            std::cerr
                << "[TEXTURE WARN] Nie znaleziono tekstury na dysku: "
                << requestedFilename
                << std::endl;

            return tex;
        }

        // std::cout
        //     << "[TEXTURE] request='"
        //     << requestedFilename
        //     << "' -> '"
        //     << resolvedPath
        //     << "'"
        //     << std::endl;

        int width = 0;
        int height = 0;
        int channels = 0;

        stbi_uc* pixels = stbi_load(
            resolvedPath.c_str(),
            &width,
            &height,
            &channels,
            STBI_rgb_alpha
        );

        if (!pixels)
        {
            std::cerr
                << "[TEXTURE ERROR] Nie udało się wczytać pliku: "
                << resolvedPath
                << "\n"
                << "                 stb_image: "
                << stbi_failure_reason()
                << std::endl;

            return tex;
        }

        tex.width = width;
        tex.height = height;
        tex.path = resolvedPath;

        glGenTextures(1, &tex.id);
        glBindTexture(GL_TEXTURE_2D, tex.id);

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MIN_FILTER,
            GL_LINEAR_MIPMAP_LINEAR
        );

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MAG_FILTER,
            GL_LINEAR
        );

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_S,
            GL_REPEAT
        );

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_T,
            GL_REPEAT
        );

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            width,
            height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            pixels
        );

        glGenerateMipmap(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, 0);

        stbi_image_free(pixels);

        tex.valid = true;

        // std::cout
        //     << "  -> SUCCESS: Wczytano "
        //     << requestedFilename
        //     << " ("
        //     << tex.width
        //     << "x"
        //     << tex.height
        //     << ")"
        //     << std::endl;

        return tex;
    }

};