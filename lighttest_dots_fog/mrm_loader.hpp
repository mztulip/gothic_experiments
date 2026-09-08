#pragma once

#include <glm/glm.hpp>
#include <zenkit/MultiResolutionMesh.hh>
#include <zenkit/Vfs.hh>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>

// Vertex jest juz zdefiniowany w zen_loader.hpp (pos, normal, uv) -
// ten plik musi byc dolaczony PO zen_loader.hpp.

// ---------------------------------------------------------------------
// Zamienia rozszerzenie na MRM (analogicznie do OpenGothic:
// implLoadMeshMain -> FileExt::exchangeExt(name, "3DS", "MRM"))
// ---------------------------------------------------------------------
static std::string toMrmName(const std::string& visualName)
{
    std::string name = visualName;

    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos)
        name = name.substr(0, dot) + ".MRM";
    else
        name += ".MRM";

    return name;
}

// ---------------------------------------------------------------------
// Laduje MRM z zamontowanego Vfs i splaszcza go do tego samego
// formatu Vertex (pos, normal, uv), ktorego uzywa loader 3ds -
// dzieki temu reszta pipeline'u (upload GPU, cache) jest wspolna.
// ---------------------------------------------------------------------
static bool loadMrmMesh(
    zenkit::Vfs& vfs,
    const std::string& visualName,
    std::vector<Vertex>& outVerts)
{
    if (visualName.empty())
        return false;

    std::string mrmName = toMrmName(visualName);

    const zenkit::VfsNode* node = vfs.find(mrmName);
    if (node == nullptr)
    {
        printf("[MRM] Nie znaleziono w VFS: %s\n", mrmName.c_str());
        return false;
    }

    zenkit::MultiResolutionMesh mrm;

    try
    {
        auto reader = node->open_read();
        mrm.load(reader.get());
    }
    catch (const std::exception& e)
    {
        printf("[MRM] Blad parsowania %s: %s\n", mrmName.c_str(), e.what());
        return false;
    }

    outVerts.clear();

    for (const auto& sub : mrm.sub_meshes)
    {
        for (const auto& tri : sub.triangles)
        {
            // Kolejnosc 0,2,1 - tak samo jak przy world_mesh w zen_loader.hpp,
            // zeby zachowac spojne nawijanie trojkatow wzgledem zenPosToGL.
            const uint16_t order[3] = { tri.wedges[0], tri.wedges[2], tri.wedges[1] };

            for (uint16_t wIdx : order)
            {
                if (wIdx >= sub.wedges.size())
                    continue;

                const auto& wedge = sub.wedges[wIdx];

                if (wedge.index >= mrm.positions.size())
                    continue;

                const auto& p = mrm.positions[wedge.index];

                Vertex v;
                v.pos    = zenPosToGL(p.x, p.y, p.z);
                v.normal = zenPosToGL(wedge.normal.x, wedge.normal.y, wedge.normal.z);
                v.uv     = glm::vec2(wedge.texture.x, 1.0f - wedge.texture.y);

                outVerts.push_back(v);
            }
        }
    }

    if (outVerts.empty())
    {
        printf("[MRM] Pusty mesh po sparsowaniu: %s\n", mrmName.c_str());
        return false;
    }

    return true;
}