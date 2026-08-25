#pragma once

#include <glm/glm.hpp>
#include <string>

// ---------------------------------------------------------------------
// Wczytywanie prawdziwych swiatel z pliku .ZEN (ZenKit) - te same wartosci
// Range/kolor/preset co w grze, 
// ---------------------------------------------------------------------
struct LoadedLight {
  glm::vec3   pos;
  float       range;
  glm::vec3   color;
  std::string preset;
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
    if(clip.w <= 0.01f)
    {
      // rog za kamera - bezpieczny fallback: caly ekran dla tego swiatla
      x0=0; y0=0; x1=fbw; y1=fbh;
      return true;
    }
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    float sx = (ndc.x*0.5f+0.5f) * fbw;
    float sy = (1.f-(ndc.y*0.5f+0.5f)) * fbh;
    minX = std::min(minX, sx); maxX = std::max(maxX, sx);
    minY = std::min(minY, sy); maxY = std::max(maxY, sy);
  }

  x0 = std::max(0, int(std::floor(minX)));
  y0 = std::max(0, int(std::floor(minY)));
  x1 = std::min(fbw, int(std::ceil(maxX)));
  y1 = std::min(fbh, int(std::ceil(maxY)));
  return (x1 > x0) && (y1 > y0);
}