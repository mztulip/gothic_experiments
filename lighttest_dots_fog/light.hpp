#pragma once

#include <glm/glm.hpp>
#include <string>

// ---------------------------------------------------------------------
// Presety swiatla - zmierzone/przyjete wartosci z HELMS.ZEN (patrz rozmowa)
// ---------------------------------------------------------------------
struct Preset {
  const char* name;
  float       range;
  glm::vec3   color;      // baza koloru (0..1)
  };

static const Preset PRESETS[] = {
  {"FIRESMALL",  200.f, {1.00f, 0.29f, 0.00f}}, // 255,73,0
  {"FIRE",       300.f, {1.00f, 0.00f, 0.00f}}, // klatka 255,0,0 (najbardziej "problematyczna")
  {"JUSTWHITE",  700.f,  {1.00f, 1.00f, 0.68f}}, // 255,255,173
  {"WHITEBLEND",2000.f,  {1.00f, 1.00f, 0.58f}}, // 255,255,148
  {"AURA_650",   650.f,  {0.00f, 0.00f, 0.55f}}, // 0,0,139
  {"AURA_3000", 3000.f, {0.32f, 0.66f, 0.84f}}, // 81,168,214
  };


// ---------------------------------------------------------------------
// Wczytywanie prawdziwych swiatel z pliku .ZEN (ZenKit) - te same wartosci
// Range/kolor/preset co w grze, 
// ---------------------------------------------------------------------
struct LoadedLight {
  glm::vec3   pos;
  float       range;
  glm::vec3   color;
  std::string preset;
  bool        isStatic = false;
};

// Rzutuje bounding-box swiatla (pos +- range) na ekran, zwraca prostokat
// scissor. Jesli jakikolwiek rog bboxa jest za kamera, bezpiecznie
// fallbackujemy do calego ekranu (rzadki przypadek - kamera blisko/w
// srodku swiatla).
static bool lightScissorRect(const glm::vec3& pos, float range,
                              const glm::mat4& view, const glm::mat4& proj,
                              int fbw, int fbh,
                              int& x0, int& y0, int& x1, int& y1)
{
  // Sprawdzenie w przestrzeni widoku (przed projekcja) - odpornie na
  // przypadek gdy bounding-box swiatla jest blisko/przecina plaszczyzne
  // przechodzaca przez kamere (prostopadla do kierunku patrzenia).
  // Kamera patrzy w kierunku -Z (konwencja OpenGL) - punkty przed kamera
  // maja viewZ < 0.
  glm::vec4 viewCenter = view * glm::vec4(pos, 1.0f);
  float viewZ = viewCenter.z;

  // Jesli srodek swiatla nie jest wystarczajaco daleko PRZED kamera
  // (z zapasem rownym range, zeby cala kula na pewno miescila sie
  // przed plaszczyzna kamery) - rzutowanie rogow byloby zawodne.
  // Bezpieczny fallback: caly ekran.
  const float safetyMargin = 1.05f; // 5% zapasu na bezpieczenstwo numeryczne
  if(viewZ > -range * safetyMargin)
  {
    x0 = 0; y0 = 0; x1 = fbw; y1 = fbh;
    return true;
  }

  glm::vec3 corners[8];
  int idx = 0;
  for(int dx=-1; dx<=1; dx+=2)
  for(int dy=-1; dy<=1; dy+=2)
  for(int dz=-1; dz<=1; dz+=2)
    corners[idx++] = pos + glm::vec3(dx*range, dy*range, dz*range);

  float minX=1e9f, maxX=-1e9f, minY=1e9f, maxY=-1e9f;

  for(auto& c : corners)
  {
    glm::vec4 clip = proj * view * glm::vec4(c, 1.f);
    // dzieki wczesniejszemu sprawdzeniu viewZ, w tym miejscu clip.w
    // powinno byc bezpiecznie dodatnie i "duze" dla wszystkich rogow
    glm::vec3 ndc = glm::vec3(clip) / clip.w;

    float sx = (ndc.x*0.5f+0.5f) * fbw;
    float sy = (ndc.y*0.5f+0.5f) * fbh; // <-- BEZ odwrocenia Y (glScissor: origin dolny-lewy)

    minX = std::min(minX, sx); maxX = std::max(maxX, sx);
    minY = std::min(minY, sy); maxY = std::max(maxY, sy);
  }

  x0 = std::max(0, int(std::floor(minX)));
  y0 = std::max(0, int(std::floor(minY)));
  x1 = std::min(fbw, int(std::ceil(maxX)));
  y1 = std::min(fbh, int(std::ceil(maxY)));
  return (x1 > x0) && (y1 > y0);
}