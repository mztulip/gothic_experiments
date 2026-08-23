
/*
g++ -std=c++20 \
  -I /home/mz/gothic/zen/OpenGothic/lib/ZenKit/include \
  -I /home/mz/gothic/zen/OpenGothic/lib/ZenKit/vendor/glm \
  list_lights.cc \
  /home/mz/gothic/zen/OpenGothic/build/lib/ZenKit/libzenkit.a \
  /home/mz/gothic/zen/OpenGothic/build/lib/ZenKit/vendor/libsquish/CMakeFiles/squish.dir/*.o \
  -o list_lights
*/
// ./list_lights ./toteninsel/TOTENINSEL.ZEN

// list_lights.cc
// Wypisuje wszystkie zCVobLight (i ich parametry) z podanego pliku .ZEN
// Kompilacja / użycie -> patrz instrukcja w rozmowie.

#include <zenkit/World.hh>
#include <zenkit/Archive.hh>
#include <zenkit/Stream.hh>
#include <zenkit/vobs/Light.hh>
#include <zenkit/vobs/VirtualObject.hh>

#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

static const char* lightTypeName(zenkit::LightType t) {
  switch(t) {
    case zenkit::LightType::POINT: return "POINT";
    case zenkit::LightType::SPOT:  return "SPOT";
    default:                       return "RESERVED/UNKNOWN";
    }
  }

static const char* qualityName(zenkit::LightQuality q) {
  switch(q) {
    case zenkit::LightQuality::HIGH:   return "HIGH";
    case zenkit::LightQuality::MEDIUM: return "MEDIUM";
    case zenkit::LightQuality::LOW:    return "LOW";
    default:                           return "?";
    }
  }

static void printLight(const zenkit::VLight& l) {
  std::cout << "----------------------------------------\n";
  std::cout << "vob_name     : " << l.vob_name << "\n";
  std::cout << "preset       : " << l.preset << "\n";
  std::cout << "position     : (" << l.position.x << ", " << l.position.y << ", " << l.position.z << ")\n";
  std::cout << "type         : " << lightTypeName(l.light_type) << "\n";
  std::cout << "range        : " << l.range << "\n";
  std::cout << "color(RGBA)  : (" << int(l.color.r) << ", " << int(l.color.g) << ", "
             << int(l.color.b) << ", " << int(l.color.a) << ")\n";
  std::cout << "cone_angle   : " << l.cone_angle << "\n";
  std::cout << "is_static    : " << (l.is_static ? "true" : "false") << "\n";
  std::cout << "quality      : " << qualityName(l.quality) << "\n";
  std::cout << "on (dynamic) : " << (l.on ? "true" : "false") << "\n";
  std::cout << "can_move     : " << (l.can_move ? "true" : "false") << "\n";
  std::cout << "range_anim   : " << l.range_animation_scale.size() << " frames, fps="
             << l.range_animation_fps << ", smooth=" << l.range_animation_smooth << "\n";
  std::cout << "color_anim   : " << l.color_animation_list.size() << " frames, fps="
             << l.color_animation_fps << ", smooth=" << l.color_animation_smooth << "\n";
  std::cout << "lensflare_fx : " << l.lensflare_fx << "\n";
  }

static void walk(const std::shared_ptr<zenkit::VirtualObject>& vob, int& count) {
  if(vob->type==zenkit::VirtualObjectType::zCVobLight) {
    // VLight dziedziczy po VirtualObject i LightPreset jednocześnie -
    // ten sam wzorzec rzutowania stosuje OpenGothic w game/world/objects/vob.cpp
    const auto& light = static_cast<const zenkit::VLight&>(*vob);
    printLight(light);
    ++count;
    }
  for(auto& child : vob->children)
    walk(child, count);
  }

int main(int argc, char** argv) {
  if(argc!=2) {
    std::cerr << "Usage: " << argv[0] << " path/to/TOTENINSEL.ZEN\n";
    return 1;
    }

  zenkit::World world;
  try {
    auto reader = zenkit::Read::from(std::filesystem::path(argv[1]));
    world.load(reader.get());
    }
  catch(const std::exception& e) {
    std::cerr << "Blad wczytywania pliku: " << e.what() << "\n";
    return 1;
    }

  std::cout << "Found " << world.world_vobs.size() << " vobs highest level.\n";

  int count = 0;
  for(auto& vob : world.world_vobs)
    walk(vob, count);

  std::cout << "\n==========================================\n";
  std::cout << "Found  " << count << " lights (zCVobLight).\n";
  return 0;
  }