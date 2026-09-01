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


#include "env_load.hpp"
#include "texture_loader.hpp"
#include "3ds_loader.h"
#include "zen_debug.hpp"

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
    //Narazie tego nie używam bo po tej transformacji obiekty mam w powietrzu
    glm::mat4 meshLocalTransform{1.f};

};


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


static void walkVobs(
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
 
      obj.rotation = glm::mat4(1.f);

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
        walkVobs(c, out);
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
        {
            debugPrintVob(vob, 0);
            walkVobs(vob, out);
        }
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

