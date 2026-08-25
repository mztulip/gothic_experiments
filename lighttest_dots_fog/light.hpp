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