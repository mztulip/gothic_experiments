#ifndef TEXTURE_LOADER_H
#define TEXTURE_LOADER_H

#include <epoxy/gl.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <string>
#include <iostream>

struct Texture2D {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
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

class TextureLoader {
public:
    static Texture2D loadFromFile(const std::string& filepath, bool flipVertically = false) {
        Texture2D tex;
        tex.path = filepath;

        int w, h, comp;
        stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);
        unsigned char* data = stbi_load(filepath.c_str(), &w, &h, &comp, 4);

        if (data) {
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
        } else {
            tex.valid = false;
        }

        return tex;
    }
};

#endif // TEXTURE_LOADER_H