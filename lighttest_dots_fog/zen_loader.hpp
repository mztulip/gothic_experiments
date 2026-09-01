#pragma once

#include <glm/glm.hpp>
#include <zenkit/World.hh>
#include <zenkit/Stream.hh>
#include <zenkit/Mesh.hh>
#include <zenkit/vobs/Light.hh>
#include <zenkit/vobs/VirtualObject.hh>
#include <string>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>


#include "texture_loader.hpp"
#include "3ds_loader.h"

static inline glm::vec3 zenPosToGL(float x, float y, float z)
{
  // ZenGin (Gothic) uzywa ukladu lewoskretnego, OpenGL prawoskretnego.
  // Negujemy DOKLADNIE JEDNA os, aby zamienic chiralnosc - tutaj X.
  // Jesli po tescie okaze sie, ze to zla os, zmien negacje na Y lub Z
  // (ale zawsze tylko jedna naraz - negacja dwoch osi to obrot, nie odbicie).
  return glm::vec3(-x, y, z);
}


static glm::mat4 zenRotationToGL(const zenkit::Mat3& r)
{
    glm::mat4 R(1.0f);

    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            R[col][row] = r[row][col];
        }
    }

    return R;
}

static glm::mat4 zenRotationToGL_nomod(const zenkit::Mat3& r)
{
    glm::mat4 R(1.0f);

    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            R[row][col] = r[row][col];
        }
    }

    return R;
}



// ---------------------------------------------------------------------
// Pomoce do budowy geometrii (podloga, kostka)
// ---------------------------------------------------------------------
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv; //współrzędne tekstury
};

struct LoadedVob
{
    glm::vec3 pos;
    glm::vec3 bboxMin;
    glm::vec3 bboxMax;

    zenkit::VirtualObjectType type;

    std::string visualName;
    std::string meshPath;

    bool showVisual = false;
    bool meshLoaded = false;

    GLuint meshVao = 0;
    GLuint meshVbo = 0;
    size_t meshVertexCount = 0;

    glm::vec3 meshMin{0.f};
    glm::vec3 meshMax{0.f};
     // Transformacja VOB-a z ZEN
    glm::mat4 rotation{1.f};
    
    // Transformacja lokalna zapisana w 3DS
    // glm::mat4 meshLocalTransform{1.f};

};



static std::string findMeshFile(
    const std::string& gothicDir,
    const std::string& visualName)
{
    if (visualName.empty())
        return {};

    namespace fs = std::filesystem;

    fs::path meshesDir =
    fs::path(gothicDir) / "_Work" / "Data" / "Meshes";

    printf("SZUKAM MESHY W: %s\n", meshesDir.string().c_str());

    if (!fs::exists(meshesDir))
    {
        printf("KATALOG MESHES NIE ISTNIEJE!\n");
        return {};
    }

    printf("KATALOG MESHES ISTNIEJE.\n");


    std::string wanted = visualName;

    std::transform(
        wanted.begin(),
        wanted.end(),
        wanted.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });

    try
    {
        for (const auto& entry :
             fs::recursive_directory_iterator(meshesDir))
        {
            if (!entry.is_regular_file())
                continue;

            std::string filename =
                entry.path().filename().string();

            std::transform(
                filename.begin(),
                filename.end(),
                filename.begin(),
                [](unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });

            if (filename == wanted)
                return entry.path().string();
        }
    }
    catch (const std::exception& e)
    {
        fprintf(stderr,
                "Blad podczas szukania mesha: %s\n",
                e.what());
    }

    return {};
}


static std::string getGothicDir()
{
    const char* env = std::getenv("GOTHIC2_DIR");

    if (!env)
    {
        printf("GOTHIC2_DIR: NIE USTAWIONA\n");
        return {};
    }

    printf("GOTHIC2_DIR RAW: %s\n", env);

    std::string path(env);

    // Usuwamy backslashe używane przez shell do escapowania spacji
    path.erase(
        std::remove(path.begin(), path.end(), '\\'),
        path.end()
    );

    printf("GOTHIC2_DIR: %s\n", path.c_str());

    return path;
}

static bool loadVobMesh(
    const std::string& gothicDir,
    const std::string& visualName,
    Mesh3DS& mesh)
{
    if (visualName.empty())
        return false;

    std::string meshPath =
        findMeshFile(gothicDir, visualName);

    if (meshPath.empty())
    {
        printf(
            "MESH NOT FOUND: %s\n",
            visualName.c_str()
        );

        return false;
    }

    printf(
        "MESH FOUND: %s -> %s\n",
        visualName.c_str(),
        meshPath.c_str()
    );

    if (!Loader3DS::load(meshPath, mesh))
    {
        printf(
            "MESH LOAD FAILED: %s\n",
            meshPath.c_str()
        );

        return false;
    }

    return true;
}


static void testLoadVobMesh(
    const std::shared_ptr<zenkit::VirtualObject>& vob)
{
    if (!vob->show_visual)
        return;

    if (!vob->visual)
        return;

    if (vob->visual->name.empty())
        return;

    if (vob->visual->name[0] == '#')
    {
        printf(
            "POMIJAM WEWNETRZNY VISUAL: '%s'\n",
            vob->visual->name.c_str()
        );

        return;
    }

    if (vob->visual->type !=
        zenkit::VisualType::MESH &&
        vob->visual->type !=
        zenkit::VisualType::MULTI_RESOLUTION_MESH)
    {
        return;
    }

    std::string gothicDir = getGothicDir();

    if (gothicDir.empty())
    {
        printf("Brak GOTHIC2_DIR\n");
        return;
    }

    std::string meshPath =
        findMeshFile(
            gothicDir,
            vob->visual->name);

    if (meshPath.empty())
    {
        printf(
            "MESH NOT FOUND: %s\n",
            vob->visual->name.c_str());

        return;
    }

    printf(
        "MESH FOUND: %s\n",
        meshPath.c_str());

    Mesh3DS mesh;

    if (!Loader3DS::load(meshPath, mesh))
    {
        printf(
            "MESH LOAD FAILED: %s\n",
            meshPath.c_str());

        return;
    }

    printf(
        "  vertices: %zu\n",
        mesh.vertices.size());

    printf(
        "  faces: %zu\n",
        mesh.faces.size());

    if (!mesh.vertices.empty())
    {
        printf(
            "  vertex[0]: %.2f %.2f %.2f\n",
            mesh.vertices[0].x,
            mesh.vertices[0].y,
            mesh.vertices[0].z);
    }

    if (!mesh.faces.empty())
    {
        printf(
            "  face[0]: %u %u %u\n",
            mesh.faces[0].a,
            mesh.faces[0].b,
            mesh.faces[0].c);
    }

    printf(
        "  bounds min: %.2f %.2f %.2f\n",
        mesh.minBounds.x,
        mesh.minBounds.y,
        mesh.minBounds.z
    );

    printf(
        "  bounds max: %.2f %.2f %.2f\n",
        mesh.maxBounds.x,
        mesh.maxBounds.y,
        mesh.maxBounds.z
    );

    printf(
        "  center: %.2f %.2f %.2f\n",
        mesh.center.x,
        mesh.center.y,
        mesh.center.z
    );

    printf(
        "  maxDimension: %.2f\n",
        mesh.maxDimension
    );

    printf(
        "  materials: %zu\n",
        mesh.materials.size()
    );

    if (mesh.materialForFace.size() != mesh.faces.size())
    {
        printf(
            "  WARNING: materialForFace != faces!\n"
        );
    }

    for (size_t i = 0; i < mesh.materialForFace.size(); ++i)
    {
        uint16_t mat = mesh.materialForFace[i];

        if (mat >= mesh.materials.size())
        {
            printf(
                "  WARNING: face[%zu] has invalid material %u\n",
                i,
                mat
            );
        }
    }

    for (size_t i = 0; i < mesh.materials.size(); ++i)
    {
        printf(
            "    material[%zu]: name='%s' texture='%s'\n",
            i,
            mesh.materials[i].name.c_str(),
            mesh.materials[i].textureFile.c_str()
        );
    }

    for (size_t i = 0;
     i < mesh.materialForFace.size();
     ++i)
    {
        printf(
            "    face[%zu] -> material %u\n",
            i,
            mesh.materialForFace[i]
        );
    }

    std::vector<size_t> materialFaceCount(
    mesh.materials.size(),
    0
);

    for (uint16_t mat : mesh.materialForFace)
    {
        if (mat < materialFaceCount.size())
            materialFaceCount[mat]++;
    }

    for (size_t i = 0; i < materialFaceCount.size(); ++i)
    {
        printf(
            "  material[%zu] '%s' -> %zu faces\n",
            i,
            mesh.materials[i].name.c_str(),
            materialFaceCount[i]
        );
    }
}


static void walkVobsForBoxes(
    const std::shared_ptr<zenkit::VirtualObject>& vob,
    std::vector<LoadedVob>& out)
{
    // printf(
    //     "VOB: %s\n"
    //     "pos = %.2f %.2f %.2f\n"
    //     "rotation:\n"
    //     "%.4f %.4f %.4f\n"
    //     "%.4f %.4f %.4f\n"
    //     "%.4f %.4f %.4f\n",
    //     vob->visual->name.c_str(),
    //     vob->position.x,
    //     vob->position.y,
    //     vob->position.z,

    //     vob->rotation[0][0],
    //     vob->rotation[0][1],
    //     vob->rotation[0][2],

    //     vob->rotation[1][0],
    //     vob->rotation[1][1],
    //     vob->rotation[1][2],

    //     vob->rotation[2][0],
    //     vob->rotation[2][1],
    //     vob->rotation[2][2]
    // );

    if (vob->type != zenkit::VirtualObjectType::zCVobLight)
    {
      LoadedVob obj;

      obj.pos = zenPosToGL(
          vob->position.x,
          vob->position.y,
          vob->position.z
      );
//       obj.pos = glm::vec3(
//     vob->position.x,
//     vob->position.y,
//     vob->position.z
// );


      // obj.rotation = zenRotationToGL(vob->rotation);
      obj.rotation = zenRotationToGL(vob->rotation);
      // obj.rotation = glm::mat4(1.f)`;

      glm::vec3 bmin = zenPosToGL(
          vob->bbox.min.x,
          vob->bbox.min.y,
          vob->bbox.min.z
      );

      glm::vec3 bmax = zenPosToGL(
          vob->bbox.max.x,
          vob->bbox.max.y,
          vob->bbox.max.z
      );

      obj.bboxMin = glm::min(bmin, bmax);
      obj.bboxMax = glm::max(bmin, bmax);


      obj.type = vob->type;
      obj.showVisual = vob->show_visual;

      if (vob->visual)
      {
          obj.visualName = vob->visual->name;

          if (
              vob->visual->type ==
                  zenkit::VisualType::MESH ||
              vob->visual->type ==
                  zenkit::VisualType::MULTI_RESOLUTION_MESH
          )
          {
              std::string gothicDir = getGothicDir();

              if (!gothicDir.empty())
              {
                  obj.meshPath =
                      findMeshFile(
                          gothicDir,
                          obj.visualName
                      );

                  obj.meshLoaded =
                      !obj.meshPath.empty();
              }
          }
      }

      out.push_back(obj);
    }

    for (auto& c : vob->children)
        walkVobsForBoxes(c, out);
}


static std::vector<LoadedVob> loadVobsFromZen(const std::string& path)
{
    std::vector<LoadedVob> out;

    try
    {
        auto reader = zenkit::Read::from(path);

        zenkit::World world;
        world.load(reader.get());

        for (auto& vob : world.world_vobs)
            walkVobsForBoxes(vob, out);
    }
    catch (const std::exception& e)
    {
        fprintf(
            stderr,
            "Nie udalo sie wczytac VOB-ow z %s: %s\n",
            path.c_str(),
            e.what()
        );

        return {};
    }

    printf(
        "Wczytano %zu obiektow VOB do debugowania\n",
        out.size()
    );

    return out;
}

static glm::vec3 vobTypeColor(zenkit::VirtualObjectType type)
{
    switch (type)
    {
        case zenkit::VirtualObjectType::zCVob:
            return {1.0f, 1.0f, 0.0f}; // żółty

        case zenkit::VirtualObjectType::zCVobLevelCompo:
            return {1.0f, 0.6f, 0.0f}; // pomarańczowy

        case zenkit::VirtualObjectType::oCItem:
            return {0.0f, 1.0f, 0.0f}; // zielony

        case zenkit::VirtualObjectType::oCNpc:
            return {0.2f, 0.5f, 1.0f}; // niebieski

        case zenkit::VirtualObjectType::oCMOB:
            return {1.0f, 0.2f, 1.0f}; // różowy

        case zenkit::VirtualObjectType::oCMobInter:
            return {0.8f, 0.2f, 1.0f}; // fioletowy

        case zenkit::VirtualObjectType::oCMobContainer:
            return {0.0f, 1.0f, 1.0f}; // cyan

        case zenkit::VirtualObjectType::oCMobDoor:
            return {1.0f, 0.2f, 0.2f}; // czerwony

        case zenkit::VirtualObjectType::zCVobStartpoint:
            return {1.0f, 1.0f, 1.0f}; // biały

        case zenkit::VirtualObjectType::zCVobLight:
            return {1.0f, 0.8f, 0.1f}; // światło - złoty

        default:
            return {0.7f, 0.7f, 0.7f}; // szary
    }
}



static void walkVobsForStartPoint(const std::shared_ptr<zenkit::VirtualObject>& vob,
                                   bool& found, glm::vec3& outPos)
{
  if(found) return;
  if(vob->type==zenkit::VirtualObjectType::zCVobStartpoint)
  {
    outPos = zenPosToGL(vob->position.x, vob->position.y, vob->position.z);
    found = true;
    return;
  }
  for(auto& c : vob->children)
  {
    walkVobsForStartPoint(c, found, outPos);
    if(found) return;
  }
}

// Szuka pierwszego zCVobStartpoint w pliku .ZEN. Zwraca false, jesli go nie ma
// (nie kazdy testowy/czesciowy swiat go zawiera).
static bool findStartPointFromZen(const std::string& path, glm::vec3& outPos) {
  try {
    auto reader = zenkit::Read::from(path);
    zenkit::World world;
    world.load(reader.get());
    bool found = false;
    for(auto& vob : world.world_vobs) {
      walkVobsForStartPoint(vob, found, outPos);
      if(found) break;
      }
    if(found)
      printf("Znaleziono zCVobStartpoint: (%.1f, %.1f, %.1f)\n", outPos.x, outPos.y, outPos.z);
    else
      printf("Brak zCVobStartpoint w tym pliku - kamera wystartuje przy centroidzie swiatel.\n");
    return found;
    }
  catch(const std::exception& e) {
    fprintf(stderr, "Nie udalo sie sprawdzic startpointu w %s: %s\n", path.c_str(), e.what());
    return false;
    }
  }


static void debugLight(const zenkit::VLight& l)
{
    printf("\n========== LIGHT ==========\n");

    printf("position:     %.2f %.2f %.2f\n",
           l.position.x,
           l.position.y,
           l.position.z);

    printf("range:        %.2f\n", l.range);

    printf("color:        %d %d %d\n",
           l.color.r,
           l.color.g,
           l.color.b);

    printf("preset:       '%s'\n", l.preset.c_str());

    printf("is_static:    %s\n", l.is_static ? "true" : "false");

    printf("============================\n");
}


static const char* vobTypeName(zenkit::VirtualObjectType type)
{
    switch (type)
    {
        case zenkit::VirtualObjectType::UNKNOWN:
            return "UNKNOWN";

        case zenkit::VirtualObjectType::zCVob:
            return "zCVob";

        case zenkit::VirtualObjectType::zCVobLevelCompo:
            return "zCVobLevelCompo";

        case zenkit::VirtualObjectType::oCItem:
            return "oCItem";

        case zenkit::VirtualObjectType::oCNpc:
            return "oCNpc";

        case zenkit::VirtualObjectType::zCMoverController:
            return "zCMoverController";

        case zenkit::VirtualObjectType::zCVobScreenFX:
            return "zCVobScreenFX";

        case zenkit::VirtualObjectType::zCVobStair:
            return "zCVobStair";

        case zenkit::VirtualObjectType::zCPFXController:
            return "zCPFXController";

        case zenkit::VirtualObjectType::zCVobAnimate:
            return "zCVobAnimate";

        case zenkit::VirtualObjectType::zCVobLensFlare:
            return "zCVobLensFlare";

        case zenkit::VirtualObjectType::zCVobLight:
            return "zCVobLight";

        case zenkit::VirtualObjectType::zCVobSpot:
            return "zCVobSpot";

        case zenkit::VirtualObjectType::zCVobStartpoint:
            return "zCVobStartpoint";

        case zenkit::VirtualObjectType::zCMessageFilter:
            return "zCMessageFilter";

        case zenkit::VirtualObjectType::zCCodeMaster:
            return "zCCodeMaster";

        case zenkit::VirtualObjectType::zCTriggerWorldStart:
            return "zCTriggerWorldStart";

        case zenkit::VirtualObjectType::zCCSCamera:
            return "zCCSCamera";

        case zenkit::VirtualObjectType::zCCamTrj_KeyFrame:
            return "zCCamTrj_KeyFrame";

        case zenkit::VirtualObjectType::oCTouchDamage:
            return "oCTouchDamage";

        case zenkit::VirtualObjectType::zCTriggerUntouch:
            return "zCTriggerUntouch";

        case zenkit::VirtualObjectType::zCEarthquake:
            return "zCEarthquake";

        case zenkit::VirtualObjectType::oCMOB:
            return "oCMOB";

        case zenkit::VirtualObjectType::oCMobInter:
            return "oCMobInter";

        case zenkit::VirtualObjectType::oCMobBed:
            return "oCMobBed";

        case zenkit::VirtualObjectType::oCMobFire:
            return "oCMobFire";

        case zenkit::VirtualObjectType::oCMobLadder:
            return "oCMobLadder";

        case zenkit::VirtualObjectType::oCMobSwitch:
            return "oCMobSwitch";

        case zenkit::VirtualObjectType::oCMobWheel:
            return "oCMobWheel";

        case zenkit::VirtualObjectType::oCMobContainer:
            return "oCMobContainer";

        case zenkit::VirtualObjectType::oCMobDoor:
            return "oCMobDoor";

        case zenkit::VirtualObjectType::zCTrigger:
            return "zCTrigger";

        case zenkit::VirtualObjectType::zCTriggerList:
            return "zCTriggerList";

        case zenkit::VirtualObjectType::oCTriggerScript:
            return "oCTriggerScript";

        case zenkit::VirtualObjectType::oCTriggerChangeLevel:
            return "oCTriggerChangeLevel";

        case zenkit::VirtualObjectType::oCCSTrigger:
            return "oCCSTrigger";

        case zenkit::VirtualObjectType::zCMover:
            return "zCMover";

        case zenkit::VirtualObjectType::zCVobSound:
            return "zCVobSound";

        case zenkit::VirtualObjectType::zCVobSoundDaytime:
            return "zCVobSoundDaytime";

        case zenkit::VirtualObjectType::oCZoneMusic:
            return "oCZoneMusic";

        case zenkit::VirtualObjectType::oCZoneMusicDefault:
            return "oCZoneMusicDefault";

        case zenkit::VirtualObjectType::zCZoneZFog:
            return "zCZoneZFog";

        case zenkit::VirtualObjectType::zCZoneZFogDefault:
            return "zCZoneZFogDefault";

        case zenkit::VirtualObjectType::zCZoneVobFarPlane:
            return "zCZoneVobFarPlane";

        case zenkit::VirtualObjectType::zCZoneVobFarPlaneDefault:
            return "zCZoneVobFarPlaneDefault";

        default:
            return "UNKNOWN";
    }
}



static const char* visualTypeName(zenkit::VisualType type)
{
    switch (type)
    {
        case zenkit::VisualType::DECAL:
            return "DECAL";

        case zenkit::VisualType::MESH:
            return "MESH";

        case zenkit::VisualType::MULTI_RESOLUTION_MESH:
            return "MULTI_RESOLUTION_MESH";

        case zenkit::VisualType::PARTICLE_EFFECT:
            return "PARTICLE_EFFECT";

        case zenkit::VisualType::AI_CAMERA:
            return "AI_CAMERA";

        case zenkit::VisualType::MODEL:
            return "MODEL";

        case zenkit::VisualType::MORPH_MESH:
            return "MORPH_MESH";

        default:
            return "UNKNOWN";
    }
}


static void vobPrint(const std::shared_ptr<zenkit::VirtualObject>& vob)
{
  printf("\n==============================\n");

  printf("VOB type = %d (%s)\n",
       static_cast<int>(vob->type),
       vobTypeName(vob->type));

    printf("position: %.2f %.2f %.2f\n",
           vob->position.x,
           vob->position.y,
           vob->position.z);

    printf("show_visual: %s\n",
           vob->show_visual ? "true" : "false");

    printf("visual: %s\n",
           vob->visual ? "YES" : "NO");

    if (vob->visual)
    {
        printf("visual ptr: %p\n", (void*)vob->visual.get());
        printf("visual type: %d (%s)\n",
          static_cast<int>(vob->visual->type),
          visualTypeName(vob->visual->type));

        printf("visual name: '%s'\n",
              vob->visual->name.c_str());
    }
    else
    {
        printf("visual: NO\n");
    }

    printf("bbox min: %.2f %.2f %.2f\n",
       vob->bbox.min.x,
       vob->bbox.min.y,
       vob->bbox.min.z);

    printf("bbox max: %.2f %.2f %.2f\n",
          vob->bbox.max.x,
          vob->bbox.max.y,
          vob->bbox.max.z);


    printf("==============================\n");
}

static void walkVobsForDebug(
    const std::shared_ptr<zenkit::VirtualObject>& vob)
{
  vobPrint(vob);
  testLoadVobMesh(vob);


  for (auto& c : vob->children)
      walkVobsForDebug(c);
}


static void walkVobsForLights(const std::shared_ptr<zenkit::VirtualObject>& vob,
                              std::vector<LoadedLight>& out, int& staticCount)
{


  // walkVobsForDebug(vob);

  if (vob->type == zenkit::VirtualObjectType::zCVobLight)
  {
    const auto& l = static_cast<const zenkit::VLight&>(*vob);

    // debugLight(l);

    LoadedLight ll;
    ll.pos      = zenPosToGL(l.position.x, l.position.y, l.position.z);
    ll.range    = l.range;
    ll.color    = {l.color.r / 255.f, l.color.g / 255.f, l.color.b / 255.f};
    ll.preset   = l.preset;
    ll.isStatic = l.is_static; // Przypisanie informacji o statyczności

    if (l.is_static)
    {
      ++staticCount; 
    }

    out.push_back(ll);
  }

  for (auto& c : vob->children)
    walkVobsForLights(c, out, staticCount);
}

static std::vector<LoadedLight> loadLightsFromZen(const std::string& path) 
{
  std::vector<LoadedLight> out;
  int staticCount = 0;
  try {
    auto reader = zenkit::Read::from(path);
    zenkit::World world;
    world.load(reader.get());
    for(auto& vob : world.world_vobs)
      walkVobsForLights(vob, out, staticCount);
    }
  catch(const std::exception& e) {
    fprintf(stderr, "Nie udalo sie wczytac %s: %s\n", path.c_str(), e.what());
    return {};
    }

  int dynamicCount = static_cast<int>(out.size()) - staticCount;

  printf(
      "Wczytano %zu swiatel z %s: %d dynamicznych, %d statycznych\n",
      out.size(),
      path.c_str(),
      dynamicCount,
      staticCount
  );

  printf(
      "Wczytano %zu swiatel z %s (w tym %d statycznych)\n",
      out.size(),
      path.c_str(),
      staticCount
  );

  for(auto& l : out)
    printf("  preset=%-16s range=%6.1f pos=(%.1f, %.1f, %.1f) typ=%s\n",
           l.preset.empty() ? "(brak nazwy)" : l.preset.c_str(),
           l.range, l.pos.x, l.pos.y, l.pos.z, l.isStatic ? "static" : "dynamic");
  return out;
}

  struct SubMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    size_t vertexCount = 0;
    Texture2D texture;
    glm::vec3 fallbackColor{0.6f, 0.6f, 0.62f};
};

// Nowa funkcja ładowania świata podzielona na podsiatki (SubMeshes)
static std::vector<SubMesh> loadWorldSubMeshesFromZen(const std::string& path, TextureCache& texCache)
{
    std::vector<SubMesh> submeshes;
    try
    {
        auto reader = zenkit::Read::from(path);
        zenkit::World world;
        world.load(reader.get());

        const zenkit::Mesh& mesh = world.world_mesh;
        const auto& vidx = mesh.polygons.vertex_indices;
        const auto& fidx = mesh.polygons.feature_indices;
        const auto& midx = mesh.polygons.material_indices;

        if (vidx.size() != fidx.size() ||
            vidx.size() % 3 != 0 ||
            midx.size() != vidx.size() / 3)
        {
            return {};
        }


        // Grupowanie wierzchołków po indeksie materiału
        std::unordered_map<uint32_t, std::vector<Vertex>> groupedVerts;

        for (size_t i = 0; i + 2 < vidx.size(); i += 3)
        {
            size_t polyIdx = i / 3;
            uint32_t matIdx = midx[polyIdx];


            if (vidx[i + 0] >= mesh.vertices.size() ||
                vidx[i + 1] >= mesh.vertices.size() ||
                vidx[i + 2] >= mesh.vertices.size())
            {
                continue;
            }

            if (fidx[i + 0] >= mesh.features.size() ||
                fidx[i + 1] >= mesh.features.size() ||
                fidx[i + 2] >= mesh.features.size())
            {
                continue;
            }

            auto pushVertex = [&](size_t idx) {
              if (vidx[idx] >= mesh.vertices.size())
                  return;

              if (fidx[idx] >= mesh.features.size())
                  return;

                const auto& p = mesh.vertices[vidx[idx]];
                const auto& n = mesh.features[fidx[idx]].normal;
                const auto& uv = mesh.features[fidx[idx]].texture;

                glm::vec3 pos = zenPosToGL(p.x, p.y, p.z);
                glm::vec3 nrm = zenPosToGL(n.x, n.y, n.z);
                // Gothic ma odwróconą oś V w UV względem OpenGL
                glm::vec2 texcoord = glm::vec2(uv.x, 1.0f - uv.y); 

                groupedVerts[matIdx].push_back({pos, nrm, texcoord});
            };

            // Odwrócenie nawijania trójkąta z uwagi na zenPosToGL
            pushVertex(i + 0);
            pushVertex(i + 2);
            pushVertex(i + 1);
        }

        // Tworzenie VAO/VBO i ładowanie tekstur dla każdej grupy
        for (auto& [matIdx, verts] : groupedVerts)
        {
            SubMesh sm;
            sm.vertexCount = verts.size();

            if (matIdx < mesh.materials.size()) 
            {

                const auto& mat = mesh.materials[matIdx];
                sm.fallbackColor = glm::vec3(mat.color.r / 255.f, mat.color.g / 255.f, mat.color.b / 255.f);

                if (!mat.texture.empty()) {
                    sm.texture = texCache.loadTexture(mat.texture);
                }
            }

            // Tworzenie bufora OpenGL z uwzględnieniem attrib 2 (UV)
            glGenVertexArrays(1, &sm.vao);
            glGenBuffers(1, &sm.vbo);
            glBindVertexArray(sm.vao);
            glBindBuffer(GL_ARRAY_BUFFER, sm.vbo);
            glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
            glEnableVertexAttribArray(2);

            glBindVertexArray(0);
            submeshes.push_back(sm);
        }
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "Nie udało się wczytać siatki z %s: %s\n", path.c_str(), e.what());
    }

    return submeshes;
}

