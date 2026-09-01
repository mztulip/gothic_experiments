#pragma once

#include <string>


// ---------------------------------------------------------------------
// Wspolna logika oswietlenia - uzywana zarowno przez stary forward-shader
// (markery/mgla) jak i nowy deferred light-pass. Bez "#version" - jest
// doklejana programowo po nim, patrz buildFragSource() nizej.
// ---------------------------------------------------------------------
static const char* LIGHTING_COMMON_SRC = R"GLSL(
vec3 srgbDecode(vec3 c) { return pow(c, vec3(2.2)); }

vec3 acesTonemapInv(vec3 x) {
  float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
  return (-0.59 * x + 0.03 - sqrt(-1.0127 * x*x + 1.3702 * x + 0.0009)) / (2.0 * (2.43*x - 2.51));
}

vec3 textureAlbedo(vec3 rgb) {
  vec3 linear = srgbDecode(rgb);
  return acesTonemapInv(linear*0.78+0.001) * 5.0;
}

vec3 ACESFilm(vec3 x) {
  float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
  return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

// jedno miejsce prawdy dla formuly atenuacji - parametry przekazywane
// jawnie (nie przez globalne uniformy), zeby ta funkcja byla niezalezna
// od kolejnosci deklaracji uniformow w konkretnym shaderze
float attenFor(vec3 ldir, float dist, float range, int formulaMode, float lightIntensity) {
  if(formulaMode==0) {
    const float ATT1 = 0.009;
    return (dist>range) ? 0.0 : 1.0/max(ATT1*dist, 0.02);
  } else {
    float factor = dot(ldir,ldir) / (range*range);
    if(factor > 1.0) return 0.0;
    float sf = max(1.0 - factor*factor, 0.0);
    return (1.0/max(factor,0.005)) * (sf*sf) * lightIntensity;
  }
}
)GLSL";

static const char* GEOM_VERT_SRC = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;

void main() {
  vec4 world = uModel * vec4(aPos, 1.0);
  vWorldPos = world.xyz;
 // vNormal   = mat3(uModel) * aNormal;
  vNormal = mat3(transpose(inverse(uModel))) * aNormal;
  vUV       = aUV;
  gl_Position = uProj * uView * world;
}
)GLSL";

static const char* GEOM_FRAG_SRC = R"GLSL(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outWorldPos;

uniform sampler2D uTexture;
uniform bool uHasTexture;
uniform vec3 uAlbedo;

void main() {
  vec3 texColor = uHasTexture ? texture(uTexture, vUV).rgb : uAlbedo;
// vec3 texColor = vUV.xyx;
  outAlbedo   = vec4(texColor, 1.0);
  outNormal   = vec4(normalize(vNormal), 0.0);
  outWorldPos = vec4(vWorldPos, 1.0);
}
)GLSL";

// static const char* GEOM_FRAG_SRC = R"GLSL(
// #version 330 core

// in vec3 vWorldPos;
// in vec3 vNormal;
// in vec2 vUV;

// layout(location = 0) out vec4 outAlbedo;
// layout(location = 1) out vec4 outNormal;
// layout(location = 2) out vec4 outWorldPos;

// uniform sampler2D uTexture;
// uniform bool uHasTexture;
// uniform vec3 uAlbedo;

// void main()
// {
//     vec3 n = normalize(vNormal);

//     // TEST NORMALNYCH
//     outAlbedo = vec4(n * 0.5 + 0.5, 1.0);

//     // normalna do G-buffer
//     outNormal = vec4(n, 0.0);

//     outWorldPos = vec4(vWorldPos, 1.0);
// }
// )GLSL";


static const char* LIGHT_VERT_SRC = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
  vUV = aPos * 0.5 + 0.5;
  gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

static const char* LIGHT_FRAG_SRC = R"GLSL(
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uGAlbedo;
uniform sampler2D uGNormal;
uniform sampler2D uGWorldPos;

uniform vec3  uLightPos;
uniform vec3  uLightColor;
uniform float uRange;
uniform float uLightIntensity;
uniform int   uFormulaMode;
uniform int   uAmbientOnly;

void main() {
  vec3 albedo   = texture(uGAlbedo, vUV).rgb;
  vec3 normal   = texture(uGNormal, vUV).rgb;
  vec3 worldPos = texture(uGWorldPos, vUV).rgb;

  if(uAmbientOnly==1) {
    float skyLight = max(0.0, normal.y) * 0.15;
    FragColor = vec4(albedo * (0.08 + skyLight), 1.0);
    return;
  }

  vec3  ldir = uLightPos - worldPos;
  float dist = length(ldir);
  float lambert = max(0.0, dot(normalize(ldir), normal));

  float atten = attenFor(ldir, dist, uRange, uFormulaMode, uLightIntensity);

  vec3 linear = textureAlbedo(albedo);
  FragColor = vec4(linear * uLightColor * lambert * atten * 0.25, 1.0);
}
)GLSL";

// ---------------------------------------------------------------------
// Shadery - GLSL wklejony jako string, logika fragmentow 1:1 z light.frag
// ---------------------------------------------------------------------
static const char* VERT_SRC = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform bool  uIsFog;
uniform float uPointSizeBase;

out vec3 vWorldPos;
out vec3 vNormal;

void main() {
  vec4 world = uModel * vec4(aPos, 1.0);
  vWorldPos = world.xyz;
  vNormal   = mat3(uModel) * aNormal;
  vec4 viewPos = uView * world;
  gl_Position = uProj * viewPos;

  if(uIsFog) {
    float dist = length(viewPos.xyz);
    gl_PointSize = clamp(uPointSizeBase * (300.0/max(dist,1.0)), 2.0, 48.0);
  //gl_PointSize = 3.0; 

    // Kamera patrzy w kierunku -Z (konwencja OpenGL/view space).
    // Punkty zbyt blisko lub za kamera (viewPos.z blisko/powyzej 0) daja
    // zdegenerowane wspolrzedne po projekcji/dzieleniu przez w - GL_POINTS
    // nie sa clipowane na near-plane tak jak trojkaty, wiec taki punkt
    // potrafi wyrenderowac sie jako ogromny sprite na cala scene.
    // Wypychamy go recznie poza clip-space, zeby GPU go odrzucil.
    if(viewPos.z > -10.0) {
      gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
      }
    }
  }
)GLSL";

static const char* FRAG_SRC = R"GLSL(
in vec3 vWorldPos;
in vec3 vNormal;
out vec4 FragColor;

uniform vec3  uAlbedo;
uniform vec3  uLightPos;
uniform vec3  uLightColor;
uniform float uRange;
uniform float uLightIntensity;
uniform int   uFormulaMode;
uniform int   uTonemap;
uniform bool  uIsMarker;
uniform int   uAmbientOnly;
uniform bool  uIsFog;
uniform float uFogDensity;

void main()
{
    if(uIsMarker)
    {
        FragColor = vec4(uLightColor, 1.0);
        return;
    }

    float fogAlpha = 1.0;
    if(uIsFog)
    {
        vec2 c = gl_PointCoord*2.0 - 1.0;
        float r2 = dot(c,c);
        if(r2 > 1.0) discard;
        fogAlpha = 1.0 - r2;
    }

    vec3 hdrColor;

    if(uAmbientOnly==1)
    {
        float skyLight = max(0.0, normalize(vNormal).y) * 0.15;
        hdrColor = uAlbedo * (0.08 + skyLight);
    }
    else
    {
        vec3  normal = normalize(vNormal);
        vec3  ldir   = uLightPos - vWorldPos;
        float dist   = length(ldir);
        float lambert = uIsFog ? 1.0 : max(0.0, dot(normalize(ldir), normal));

        float atten = attenFor(ldir, dist, uRange, uFormulaMode, uLightIntensity);

        vec3 linear = textureAlbedo(uAlbedo);
        hdrColor = linear * uLightColor * lambert * atten * 0.25;

        if(uIsFog) hdrColor *= uFogDensity * fogAlpha;
    }

    vec3 outColor;
    if(uTonemap==1)
        outColor = ACESFilm(hdrColor);
    else
        outColor = clamp(hdrColor, 0.0, 1.0);

    FragColor = vec4(outColor, 1.0);
}
)GLSL";

// Skleja "#version" + wspolna logike oswietlenia + cialo konkretnego
// fragment shadera - GLSL nie ma #include, wiec robimy to programowo w C++.
static std::string buildFragSource(const char* body)
{
  return std::string("#version 330 core\n") + LIGHTING_COMMON_SRC + body;
}