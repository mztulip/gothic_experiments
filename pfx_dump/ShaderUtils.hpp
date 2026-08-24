#pragma once

#include <epoxy/gl.h>
#include <cstdio>

namespace ShaderUtils {
    static const char* VERT_SRC = R"GLSL(
    #version 330 core
    layout(location = 0) in vec3  aPos;
    layout(location = 1) in float aSize;
    layout(location = 2) in vec4  aColorAlpha;

    uniform mat4 uView;
    uniform mat4 uProj;
    uniform float uViewportHeight;

    out vec4 vColorAlpha;

    void main() {
      vec4 viewPos = uView * vec4(aPos, 1.0);
      gl_Position = uProj * viewPos;

      if (viewPos.z < 0.0) {
        gl_PointSize = uProj[1][1] * (aSize / -viewPos.z) * (uViewportHeight * 0.5);
      } else {
        gl_PointSize = 0.0;
      }

      vColorAlpha = aColorAlpha;
    }
    )GLSL";

    static const char* FRAG_SRC = R"GLSL(
    #version 330 core
    in vec4 vColorAlpha;
    out vec4 FragColor;

    uniform sampler2D uTexture;
    uniform bool uUseTexture;

    void main() {
      vec4 texCol = vec4(1.0);
      
      if (uUseTexture) {
        texCol = texture(uTexture, gl_PointCoord);
      } else {
        vec2 c = gl_PointCoord * 2.0 - 1.0;
        float r2 = dot(c, c);
        if (r2 > 1.0) discard;
        texCol = vec4(vec3(1.0), 1.0 - r2);
      }

      FragColor = texCol * vColorAlpha;
    }
    )GLSL";

    static const char* FLOOR_VERT_SRC = R"GLSL(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    uniform mat4 uView;
    uniform mat4 uProj;
    void main() { gl_Position = uProj * uView * vec4(aPos,1.0); }
    )GLSL";

    static const char* FLOOR_FRAG_SRC = R"GLSL(
    #version 330 core
    out vec4 FragColor;
    void main() { FragColor = vec4(0.35,0.35,0.4,0.18); }
    )GLSL";

    inline GLuint compileShader(GLenum type, const char* src) {
        GLuint sh = glCreateShader(type);
        glShaderSource(sh, 1, &src, nullptr);
        glCompileShader(sh);
        GLint ok = 0;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[4096];
            glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
            fprintf(stderr, "Blad kompilacji shadera:\n%s\n", log);
        }
        return sh;
    }

    inline GLuint linkProgram(GLuint vs, GLuint fs) {
        GLuint prog = glCreateProgram();
        glAttachShader(prog, vs);
        glAttachShader(prog, fs);
        glLinkProgram(prog);
        GLint ok = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[4096];
            glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
            fprintf(stderr, "Blad linkowania programu:\n%s\n", log);
        }
        return prog;
    }
}