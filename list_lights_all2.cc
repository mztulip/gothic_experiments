// list_lights_all.cc
//
// Przechodzi po podanych plikach .ZEN i/lub archiwach .VDF (Gothicowy virtual
// file system), wyciaga WSZYSTKIE zCVobLight ze wszystkich znalezionych
// swiatow, i wypisuje je jako CSV na stdout (do dalszej analizy/grupowania
// np. w arkuszu albo pandas), a na koniec podsumowanie per-preset na stderr.
//
// Uzycie:
//   ./list_lights_all Worlds.vdf > all_lights.csv
//   ./list_lights_all world1.zen world2.zen Worlds.vdf > all_lights.csv
//
// Kompilacja: patrz build.sh obok (taki sam schemat jak list_lights.cc).
/*

g++ -std=c++20 \
  -I /home/mz/gothic/zen/OpenGothic/lib/ZenKit/include \
  -I /home/mz/gothic/zen/OpenGothic/lib/ZenKit/vendor/glm \
  list_lights_all2.cc \
  /home/mz/gothic/zen/OpenGothic/build/lib/ZenKit/libzenkit.a \
  /home/mz/gothic/zen/OpenGothic/build/lib/ZenKit/vendor/libsquish/CMakeFiles/squish.dir/*.o \
  -o list_lights_all2

  ./list_lights_all2 \
  "/home/mz/.wine/drive_c/Program Files (x86)/JoWood/Gothic II/_Work/Data/Worlds/HELMS.ZEN" 
*/

#include <zenkit/World.hh>
#include <zenkit/Vfs.hh>
#include <zenkit/Stream.hh>
#include <zenkit/vobs/Light.hh>
#include <zenkit/vobs/VirtualObject.hh>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>

struct Stats {
  int   count       = 0;
  int   staticCount = 0;
  float rangeMin    = 1e9f;
  float rangeMax    = -1e9f;
  double rangeSum   = 0.0;
  };

static std::map<std::string, Stats> g_stats;
static long g_totalLights = 0;
static long g_totalWorlds = 0;

static const char* typeToStr(zenkit::LightType t) {
  switch(t) {
    case zenkit::LightType::POINT: return "POINT";
    case zenkit::LightType::SPOT:  return "SPOT";
    default:                       return "OTHER";
    }
  }

static std::string csvEscape(const std::string& s) {
  // proste escapowanie CSV: cudzyslowy wokol pola, podwojenie wewnetrznych cudzyslowow
  std::string out = "\"";
  for(char c : s) {
    if(c=='"') out += '"';
    out += c;
    }
  out += "\"";
  return out;
  }

static std::string formatColorAnim(const std::vector<zenkit::Color>& frames) {
  // format: r,g,b|r,g,b|... (bez alpha, nieistotne dla jasnosci)
  std::string out;
  for(size_t i = 0; i < frames.size(); ++i) {
    if(i) out += "|";
    out += std::to_string(int(frames[i].r)) + "," +
           std::to_string(int(frames[i].g)) + "," +
           std::to_string(int(frames[i].b));
    }
  return out;
  }

// prosta "jasnosc" klatki animacji (0..255 skala, suma kanalow) - do szybkiego
// wychwycenia czy w animacji jest jakas skrajnie jasna/nasycona klatka
static int frameLuma(const zenkit::Color& c) {
  return int(c.r) + int(c.g) + int(c.b);
  }

static void colorAnimMinMax(const std::vector<zenkit::Color>& frames, int& lumaMin, int& lumaMax) {
  lumaMin = 999; lumaMax = -1;
  for(auto& f : frames) {
    int l = frameLuma(f);
    lumaMin = std::min(lumaMin, l);
    lumaMax = std::max(lumaMax, l);
    }
  }

static void processLight(const std::string& worldName, const zenkit::VLight& l) {
  int lumaMin = 0, lumaMax = 0;
  colorAnimMinMax(l.color_animation_list, lumaMin, lumaMax);

  std::cout
    << csvEscape(worldName) << ","
    << csvEscape(l.preset) << ","
    << csvEscape(l.vob_name) << ","
    << l.position.x << "," << l.position.y << "," << l.position.z << ","
    << typeToStr(l.light_type) << ","
    << l.range << ","
    << int(l.color.r) << "," << int(l.color.g) << "," << int(l.color.b) << "," << int(l.color.a) << ","
    << (l.is_static ? 1 : 0) << ","
    << int(l.quality) << ","
    << (l.on ? 1 : 0) << ","
    << l.color_animation_list.size() << ","
    << l.range_animation_scale.size() << ","
    << lumaMin << "," << lumaMax << ","
    << csvEscape(formatColorAnim(l.color_animation_list))
    << "\n";

  Stats& s = g_stats[l.preset];
  s.count++;
  if(l.is_static) s.staticCount++;
  s.rangeMin = std::min(s.rangeMin, l.range);
  s.rangeMax = std::max(s.rangeMax, l.range);
  s.rangeSum += l.range;
  ++g_totalLights;
  }

static void walkVobs(const std::string& worldName, const std::shared_ptr<zenkit::VirtualObject>& vob) {
  if(vob->type==zenkit::VirtualObjectType::zCVobLight)
    processLight(worldName, static_cast<const zenkit::VLight&>(*vob));
  for(auto& child : vob->children)
    walkVobs(worldName, child);
  }

static void processWorldStream(const std::string& worldName, zenkit::Read* r) {
  zenkit::World world;
  try {
    world.load(r);
    }
  catch(const std::exception& e) {
    std::cerr << "  [pominieto " << worldName << ": " << e.what() << "]\n";
    return;
    }
  for(auto& vob : world.world_vobs)
    walkVobs(worldName, vob);
  ++g_totalWorlds;
  }

static bool hasExt(const std::string& name, const std::string& ext) {
  if(name.size() < ext.size()) return false;
  std::string tail = name.substr(name.size()-ext.size());
  std::transform(tail.begin(), tail.end(), tail.begin(), [](unsigned char c){ return std::tolower(c); });
  return tail == ext;
  }

static void processZenFile(const std::filesystem::path& path) {
  std::cerr << "Wczytuje " << path.string() << "...\n";
  try {
    auto reader = zenkit::Read::from(path);
    processWorldStream(path.filename().string(), reader.get());
    }
  catch(const std::exception& e) {
    std::cerr << "  [blad otwarcia " << path.string() << ": " << e.what() << "]\n";
    }
  }

static void walkVfsNode(const std::string& archiveName, const zenkit::VfsNode& node) {
  if(node.type()==zenkit::VfsNodeType::DIRECTORY) {
    for(auto& child : node.children())
      walkVfsNode(archiveName, child);
    return;
    }
  const std::string& name = node.name();
  if(hasExt(name, ".zen")) {
    std::string fullName = archiveName + ":" + name;
    std::cerr << "Wczytuje " << fullName << "...\n";
    auto reader = node.open_read();
    processWorldStream(fullName, reader.get());
    }
  }

static void processVdfFile(const std::filesystem::path& path) {
  std::cerr << "Montuje archiwum " << path.string() << "...\n";
  zenkit::Vfs vfs;
  try {
    vfs.mount_disk(path);
    }
  catch(const std::exception& e) {
    std::cerr << "[blad otwarcia archiwum " << path.string() << ": " << e.what() << "]\n";
    return;
    }
  walkVfsNode(path.filename().string(), vfs.root());
  }

static void printSummary() {
  std::cerr << "\n==========================================\n";
  std::cerr << "Wczytano swiatow: " << g_totalWorlds << "\n";
  std::cerr << "Razem swiatel:    " << g_totalLights << "\n\n";
  std::cerr << "Podsumowanie per-preset (posortowane wg liczby wystapien):\n";
  std::cerr << "preset;count;static;range_min;range_max;range_avg\n";

  std::vector<std::pair<std::string, Stats>> sorted(g_stats.begin(), g_stats.end());
  std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
    return a.second.count > b.second.count;
    });

  for(auto& [preset, s] : sorted) {
    std::cerr << (preset.empty() ? "(brak nazwy)" : preset) << ";"
               << s.count << ";" << s.staticCount << ";"
               << s.rangeMin << ";" << s.rangeMax << ";"
               << (s.rangeSum / s.count) << "\n";
    }
  }

int main(int argc, char** argv) {
  if(argc < 2) {
    std::cerr << "Uzycie: " << argv[0] << " plik1.zen [plik2.vdf ...]\n";
    std::cerr << "Akceptuje dowolna mieszanke pojedynczych plikow .zen i archiwow .vdf.\n";
    return 1;
    }

  std::cout << "world,preset,vob_name,pos_x,pos_y,pos_z,type,range,r,g,b,a,is_static,quality,on,color_anim_frames,range_anim_frames,anim_luma_min,anim_luma_max,color_anim_frames_rgb\n";

  for(int i = 1; i < argc; ++i) {
    std::filesystem::path p(argv[i]);
    if(!std::filesystem::exists(p)) {
      std::cerr << "[nie znaleziono: " << p.string() << "]\n";
      continue;
      }
    if(hasExt(argv[i], ".vdf"))
      processVdfFile(p);
    else if(hasExt(argv[i], ".zen"))
      processZenFile(p);
    else
      std::cerr << "[pomijam nieznany typ pliku: " << p.string() << "]\n";
    }

  printSummary();
  return 0;
  }
