#pragma once

#include <epoxy/gl.h>

struct FogBuffer
{
    GLuint vao = 0;
    GLuint vbo = 0;
    size_t count = 0;
    float range = 0.f;
    bool generated = false;
};

static void deleteFogBuffer(FogBuffer& fog)
{
    if(fog.vbo)
    {
        glDeleteBuffers(1, &fog.vbo);
        fog.vbo = 0;
    }

    if(fog.vao)
    {
        glDeleteVertexArrays(1, &fog.vao);
        fog.vao = 0;
    }

    fog.count = 0;
    fog.generated = false;
}
