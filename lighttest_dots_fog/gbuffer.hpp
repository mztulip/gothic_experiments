#pragma once

#include <epoxy/gl.h>
#include <cstdio>

struct GBuffer {
  GLuint fbo         = 0;
  GLuint texAlbedo   = 0;
  GLuint texNormal   = 0;
  GLuint texWorldPos = 0;
  GLuint rbDepth     = 0;
  int    w = 0, h = 0;

  void destroy() {
    if(fbo)         { glDeleteFramebuffers(1, &fbo);         fbo = 0; }
    if(texAlbedo)   { glDeleteTextures(1, &texAlbedo);       texAlbedo = 0; }
    if(texNormal)   { glDeleteTextures(1, &texNormal);       texNormal = 0; }
    if(texWorldPos) { glDeleteTextures(1, &texWorldPos);     texWorldPos = 0; }
    if(rbDepth)      { glDeleteRenderbuffers(1, &rbDepth);   rbDepth = 0; }
  }

  void init(int width, int height) {
    destroy();
    w = width; h = height;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    auto makeTex = [&](GLenum internalFmt) {
      GLuint t;
      glGenTextures(1, &t);
      glBindTexture(GL_TEXTURE_2D, t);
      glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      return t;
      };

    texAlbedo   = makeTex(GL_RGBA16F);
    texNormal   = makeTex(GL_RGBA16F);
    texWorldPos = makeTex(GL_RGBA32F);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texAlbedo, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, texNormal, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, texWorldPos, 0);

    GLenum drawBufs[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, drawBufs);

    glGenRenderbuffers(1, &rbDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rbDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbDepth);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr,
                "GBuffer FBO niekompletny! status = 0x%X\n",
                status);
    }
    else
    {
        printf("GBuffer FBO OK: %dx%d\n", w, h);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  // <-- TA METODA musi tu byc
  void ensureSize(int width, int height) {
    if(width == w && height == h) return;
    init(width, height);
  }
};