#pragma once

#include <glm/glm.hpp>
#include <zenkit/ModelHierarchy.hh>
#include <zenkit/ModelMesh.hh>
#include <zenkit/SoftSkinMesh.hh>
#include <zenkit/Model.hh>
#include <zenkit/Vfs.hh>
#include <zenkit/Stream.hh>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cstdio>

// Wymaga zen_loader.hpp (Vertex, SubMeshData, zenPosToGL) dolaczonego wczesniej.

// ---------------------------------------------------------------------
// Konwersja zenkit::Mat4 -> glm::mat4.
// ---------------------------------------------------------------------
static glm::mat4 zenMat4ToGL(const zenkit::Mat4& m)
{
    glm::mat4 R(1.0f);
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            R[row][col] = m[row][col];
    return R;
}

// ---------------------------------------------------------------------
// Liczy globalne (bind-pose) macierze wszystkich kosci w hierarchii.
// ---------------------------------------------------------------------
static std::vector<glm::mat4> computeGlobalBoneMatrices(const zenkit::ModelHierarchy& hierarchy)
{
    std::vector<glm::mat4> globalMats(hierarchy.nodes.size(), glm::mat4(1.0f));

    for (size_t i = 0; i < hierarchy.nodes.size(); ++i)
    {
        const auto& node = hierarchy.nodes[i];
        glm::mat4 local = zenMat4ToGL(node.transform);

        if (node.parent_index >= 0 && static_cast<size_t>(node.parent_index) < i)
        {
            globalMats[i] = globalMats[node.parent_index] * local;
        }
        else
        {
            globalMats[i] = local;
        }
    }

    return globalMats;
}

// ---------------------------------------------------------------------
// Liczy finalne (bind-pose) pozycje wierzcholkow dla jednego SoftSkinMesh.
// ---------------------------------------------------------------------
static std::vector<glm::vec3> computeSkinnedPositions(
    const zenkit::SoftSkinMesh& skin,
    const std::vector<glm::mat4>& globalBoneMats)
{
    std::vector<glm::vec3> result(skin.mesh.positions.size(), glm::vec3(0.0f));

    for (size_t vIdx = 0; vIdx < skin.weights.size(); ++vIdx)
    {
        glm::vec3 finalPos(0.0f);
        float weightSum = 0.0f;

        for (const auto& w : skin.weights[vIdx])
        {
            if (w.node_index >= skin.nodes.size())
                continue;

            int32_t hierarchyIdx = skin.nodes[w.node_index];
            if (hierarchyIdx < 0 || static_cast<size_t>(hierarchyIdx) >= globalBoneMats.size())
                continue;

            glm::vec3 localPos(w.position.x, w.position.y, w.position.z);
            glm::vec4 worldPos = globalBoneMats[hierarchyIdx] * glm::vec4(localPos, 1.0f);

            finalPos += w.weight * glm::vec3(worldPos);
            weightSum += w.weight;
        }

        if (weightSum > 0.0001f)
            finalPos /= weightSum;

        result[vIdx] = finalPos;
    }

    return result;
}

// ---------------------------------------------------------------------
// Buduje SubMeshData dla calego ModelMesh. Bind-pose, BEZ animacji.
// ---------------------------------------------------------------------
static bool buildSkinnedSubMeshes(
    const zenkit::ModelHierarchy& hierarchy,
    const zenkit::ModelMesh& modelMesh,
    std::vector<SubMeshData>& outSubMeshes)
{
    outSubMeshes.clear();

    if (modelMesh.meshes.empty() && modelMesh.attachments.empty())
    {
        printf("[MDL] ModelMesh nie ma ani SoftSkinMesh, ani attachments\n");
        return false;
    }

    std::vector<glm::mat4> globalBoneMats = computeGlobalBoneMatrices(hierarchy);

    for (const auto& skin : modelMesh.meshes)
    {
        std::vector<glm::vec3> skinnedPositions = computeSkinnedPositions(skin, globalBoneMats);

        for (const auto& sub : skin.mesh.sub_meshes)
        {
            SubMeshData data;
            data.textureName = sub.mat.texture;

            for (const auto& tri : sub.triangles)
            {
                const uint16_t order[3] = { tri.wedges[0], tri.wedges[2], tri.wedges[1] };

                for (uint16_t wIdx : order)
                {
                    if (wIdx >= sub.wedges.size())
                        continue;

                    const auto& wedge = sub.wedges[wIdx];

                    if (wedge.index >= skinnedPositions.size())
                        continue;

                    const glm::vec3& p = skinnedPositions[wedge.index];

                    Vertex v;
                    v.pos    = zenPosToGL(p.x, p.y, p.z);
                    v.normal = zenPosToGL(wedge.normal.x, wedge.normal.y, wedge.normal.z);
                    v.uv     = glm::vec2(wedge.texture.x, wedge.texture.y);

                    data.verts.push_back(v);
                }
            }

            if (!data.verts.empty())
                outSubMeshes.push_back(std::move(data));
        }
    }

    // ---------------------------------------------------------------------
    // Attachments - sztywne kawalki geometrii przypiete do konkretnej kosci
    // po nazwie (np. wieczko skrzyni, deska stolu). Bez blendowania wag -
    // jedna, sztywna macierz danej kosci.
    // ---------------------------------------------------------------------
    for (const auto& [attachName, attachMesh] : modelMesh.attachments)
    {
        int32_t nodeIdx = -1;
        for (size_t i = 0; i < hierarchy.nodes.size(); ++i)
        {
            if (hierarchy.nodes[i].name == attachName)
            {
                nodeIdx = static_cast<int32_t>(i);
                break;
            }
        }

        if (nodeIdx < 0 || static_cast<size_t>(nodeIdx) >= globalBoneMats.size())
        {
            printf("[MDL] Attachment '%s' - nie znaleziono pasujacej kosci\n", attachName.c_str());
            continue;
        }

        const glm::mat4& boneMat = globalBoneMats[nodeIdx];

        for (const auto& sub : attachMesh.sub_meshes)
        {
            SubMeshData data;
            data.textureName = sub.mat.texture;

            for (const auto& tri : sub.triangles)
            {
                const uint16_t order[3] = { tri.wedges[0], tri.wedges[2], tri.wedges[1] };

                for (uint16_t wIdx : order)
                {
                    if (wIdx >= sub.wedges.size())
                        continue;

                    const auto& wedge = sub.wedges[wIdx];

                    if (wedge.index >= attachMesh.positions.size())
                        continue;

                    const auto& rawPos = attachMesh.positions[wedge.index];
                    glm::vec4 transformed = boneMat * glm::vec4(rawPos.x, rawPos.y, rawPos.z, 1.0f);

                    Vertex v;
                    v.pos    = zenPosToGL(transformed.x, transformed.y, transformed.z);
                    v.normal = zenPosToGL(wedge.normal.x, wedge.normal.y, wedge.normal.z);
                    v.uv     = glm::vec2(wedge.texture.x, 1.0f - wedge.texture.y);

                    data.verts.push_back(v);
                }
            }

            if (!data.verts.empty())
                outSubMeshes.push_back(std::move(data));
        }
    }

    if (outSubMeshes.empty())
    {
        printf("[MDL] Brak geometrii po skinningu\n");
        return false;
    }

    printf("[MDL]   -> %zu submeshy (bind-pose, bez animacji)\n", outSubMeshes.size());
    return true;
}

// ---------------------------------------------------------------------
// Ladowanie calego .MDL (mesh+hierarchia w jednym pliku).
// Priorytet: dysk (_compiled) > VFS - tak samo jak przy MRM.
// ---------------------------------------------------------------------
static std::string toMdlName(const std::string& visualName)
{
    std::string name = visualName;
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos)
        name = name.substr(0, dot) + ".MDL";
    else
        name += ".MDL";
    return name;
}

static bool loadMdlMesh(
    zenkit::Vfs& vfs,
    const std::string& gothicDir,
    const std::string& visualName,
    std::vector<SubMeshData>& outSubMeshes)
{
    if (visualName.empty())
        return false;

    std::string mdlName = toMdlName(visualName);

    namespace fs = std::filesystem;
    fs::path diskPath = fs::path(gothicDir) / "_Work" / "Data" / "Meshes" / "_compiled" / mdlName;

    std::unique_ptr<zenkit::Read> reader;
    zenkit::Model model;

    if (fs::exists(diskPath))
    {
        printf("[LOAD MDL] (dysk) Parsowanie: %s (VOB: %s)\n", diskPath.string().c_str(), visualName.c_str());
        fflush(stdout);

        try
        {
            reader = zenkit::Read::from(diskPath.string());
            model.load(reader.get());
        }
        catch (const std::exception& e)
        {
            printf("[MDL] Blad parsowania z dysku %s: %s\n", diskPath.string().c_str(), e.what());
            return false;
        }
    }
    else
    {
        const zenkit::VfsNode* node = vfs.find(mdlName);
        if (node == nullptr)
        {
            printf("[MDL] Nie znaleziono ani na dysku, ani w VFS: %s\n", mdlName.c_str());
            return false;
        }

        printf("[LOAD MDL] (VDF) Parsowanie: %s (VOB: %s)\n", mdlName.c_str(), visualName.c_str());
        fflush(stdout);

        try
        {
            reader = node->open_read();
            model.load(reader.get());
        }
        catch (const std::exception& e)
        {
            printf("[MDL] Blad parsowania %s: %s\n", mdlName.c_str(), e.what());
            return false;
        }
    }

    return buildSkinnedSubMeshes(model.hierarchy, model.mesh, outSubMeshes);
}