#pragma once

#include <glm/glm.hpp>
#include <zenkit/World.hh>
#include <zenkit/Stream.hh>
#include <zenkit/Mesh.hh>
#include <zenkit/vobs/Light.hh>
#include <zenkit/vobs/VirtualObject.hh>
#include <string>
#include <algorithm>

#include "texture_loader.hpp"

static inline glm::vec3 zenPosToGL(float x, float y, float z)
{
  // ZenGin (Gothic) uzywa ukladu lewoskretnego, OpenGL prawoskretnego.
  // Negujemy DOKLADNIE JEDNA os, aby zamienic chiralnosc - tutaj X.
  // Jesli po tescie okaze sie, ze to zla os, zmien negacje na Y lub Z
  // (ale zawsze tylko jedna naraz - negacja dwoch osi to obrot, nie odbicie).
  return glm::vec3(-x, y, z);
}

// ---------------------------------------------------------------------
// Pomoce do budowy geometrii (podloga, kostka)
// ---------------------------------------------------------------------
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv; //współrzędne tekstury
};


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
                               std::vector<LoadedLight>& out, int& skippedStatic)
{
  if(vob->type==zenkit::VirtualObjectType::zCVobLight)
  {
    const auto& l = static_cast<const zenkit::VLight&>(*vob);
    if(l.is_static)
    {
      ++skippedStatic;
    }
    else
    {
      LoadedLight ll;
      ll.pos     = zenPosToGL(l.position.x, l.position.y, l.position.z);
      ll.range   = l.range;
      ll.color   = {l.color.r/255.f, l.color.g/255.f, l.color.b/255.f};
      ll.preset  = l.preset;
      out.push_back(ll);
    }
  }
  for(auto& c : vob->children)
    walkVobsForLights(c, out, skippedStatic);
}

static std::vector<LoadedLight> loadLightsFromZen(const std::string& path) {
  std::vector<LoadedLight> out;
  int skippedStatic = 0;
  try {
    auto reader = zenkit::Read::from(path);
    zenkit::World world;
    world.load(reader.get());
    for(auto& vob : world.world_vobs)
      walkVobsForLights(vob, out, skippedStatic);
    }
  catch(const std::exception& e) {
    fprintf(stderr, "Nie udalo sie wczytac %s: %s\n", path.c_str(), e.what());
    return {};
    }
  printf("Wczytano %zu dynamicznych swiatel z %s (pominieto %d statycznych - sa baked w vertex colors)\n",
         out.size(), path.c_str(), skippedStatic);
  for(auto& l : out)
    printf("  preset=%-16s range=%6.1f pos=(%.1f, %.1f, %.1f)\n",
           l.preset.empty() ? "(brak nazwy)" : l.preset.c_str(),
           l.range, l.pos.x, l.pos.y, l.pos.z);
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

        if(vidx.size() != fidx.size() || vidx.size() % 3 != 0) return {};

        // Grupowanie wierzchołków po indeksie materiału
        std::unordered_map<int32_t, std::vector<Vertex>> groupedVerts;

        for (size_t i = 0; i + 2 < vidx.size(); i += 3)
        {
            size_t polyIdx = i / 3;
            uint32_t matIdx = midx[polyIdx];

            auto pushVertex = [&](size_t idx) {
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