#pragma once

#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <string>
#include <memory>
#include <algorithm>
#include <cctype>
#include <filesystem>

#include <glm/glm.hpp>
#include <epoxy/gl.h>

namespace fs = std::filesystem;

struct VertexLoader {
    float x, y, z;
};

struct Face
{
    uint16_t a, b, c;
    uint16_t smoothingGroup = 0;
};

struct Mesh3DSMaterial
{
    std::string name;
    std::string textureFile;
};

struct Mesh3DS
{
    std::vector<VertexLoader> vertices;
    std::vector<Face> faces;
    std::vector<Mesh3DSMaterial> materials;
    std::vector<uint16_t> materialForFace;

    glm::vec3 minBounds{0.0f};
    glm::vec3 maxBounds{0.0f};
    glm::vec3 center{0.0f};
    float maxDimension = 1.0f;

    glm::mat4 localTransform{1.0f};
    bool hasLocalTransform = false;
};

class Loader3DS {
public:
    static bool load(const std::string& filepath, Mesh3DS& outMesh) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open())
            return false;

        outMesh.vertices.clear();
        outMesh.faces.clear();
        outMesh.materials.clear();
        outMesh.materialForFace.clear();

        uint32_t fileSize = static_cast<uint32_t>(getFileSize(file));
        parseChunk(file, fileSize, outMesh, -1, 0);

        if (outMesh.vertices.empty())
            return false;

        glm::vec3 minB(outMesh.vertices[0].x, outMesh.vertices[0].y, outMesh.vertices[0].z);
        glm::vec3 maxB = minB;

        for (const auto& v : outMesh.vertices) {
            glm::vec3 p(v.x, v.y, v.z);
            minB = glm::min(minB, p);
            maxB = glm::max(maxB, p);
        }

        outMesh.minBounds = minB;
        outMesh.maxBounds = maxB;
        outMesh.center = (minB + maxB) * 0.5f;

        glm::vec3 size = maxB - minB;
        outMesh.maxDimension = std::max(size.x, std::max(size.y, size.z));

        return true;
    }

private:
    static std::streampos getFileSize(std::ifstream& file) {
        file.seekg(0, std::ios::end);
        std::streampos size = file.tellg();
        file.seekg(0, std::ios::beg);
        return size;
    }

    static std::string readString(std::ifstream& file, uint32_t endPos) {
        std::string result;
        char ch;
        while (static_cast<uint32_t>(file.tellg()) < endPos && file.get(ch) && ch != '\0') {
            result += ch;
        }
        return result;
    }

    static int createMaterial(Mesh3DS& mesh) {
        mesh.materials.push_back({});
        return static_cast<int>(mesh.materials.size() - 1);
    }

    static int findMaterial(const Mesh3DS& mesh, const std::string& name) {
        for (size_t i = 0; i < mesh.materials.size(); ++i) {
            if (mesh.materials[i].name == name)
                return static_cast<int>(i);
        }
        return -1;
    }

    static void parseChunk(
        std::ifstream& file,
        uint32_t endPos,
        Mesh3DS& mesh,
        int currentMaterial,
        uint16_t vertexOffset)
    {
        while (static_cast<uint32_t>(file.tellg()) < endPos && file.good()) {
            uint32_t chunkStart = static_cast<uint32_t>(file.tellg());
            uint16_t chunkId = 0;
            uint32_t chunkLength = 0;

            file.read(reinterpret_cast<char*>(&chunkId), sizeof(chunkId));
            file.read(reinterpret_cast<char*>(&chunkLength), sizeof(chunkLength));

            // Zabezpieczenie przed uszkodzonym rozmiarem chunka
            if (chunkLength < 6 || chunkStart + chunkLength > endPos) {
                file.seekg(endPos, std::ios::beg);
                break;
            }

            uint32_t nextChunk = chunkStart + chunkLength;

            switch (chunkId) {
                case 0x4D4D: // MAIN3DS
                case 0x3D3D: // EDIT3DS
                case 0x4100: // N_TRI_OBJECT
                case 0xA200: // MAT_TEXMAP
                {
                    parseChunk(file, nextChunk, mesh, currentMaterial, vertexOffset);
                    break;
                }

                case 0x4000: // TRI_OBJECT (zawiera nazwę obiektu)
                {
                    readString(file, nextChunk); // Odczytaj i pomijamy nazwę
                    uint16_t currentVertexOffset = static_cast<uint16_t>(mesh.vertices.size());
                    parseChunk(file, nextChunk, mesh, currentMaterial, currentVertexOffset);
                    break;
                }

                case 0xAFFF: // MATERIAL_BLOCK
                {
                    int materialIndex = createMaterial(mesh);
                    parseChunk(file, nextChunk, mesh, materialIndex, vertexOffset);
                    break;
                }

                case 0xA000: // MAT_NAME
                {
                    std::string matName = readString(file, nextChunk);
                    if (currentMaterial >= 0 && currentMaterial < static_cast<int>(mesh.materials.size())) {
                        mesh.materials[currentMaterial].name = matName;
                    }
                    break;
                }

                case 0xA300: // MAT_MAPNAME
                {
                    std::string texName = readString(file, nextChunk);
                    if (currentMaterial >= 0 && currentMaterial < static_cast<int>(mesh.materials.size())) {
                        mesh.materials[currentMaterial].textureFile = texName;
                    }
                    break;
                }

                case 0x4110: // POINT_ARRAY
                {
                    uint16_t numVertices = 0;
                    file.read(reinterpret_cast<char*>(&numVertices), sizeof(numVertices));
                    
                    size_t startIdx = mesh.vertices.size();
                    mesh.vertices.resize(startIdx + numVertices);
                    file.read(reinterpret_cast<char*>(&mesh.vertices[startIdx]), numVertices * sizeof(VertexLoader));
                    break;
                }

                case 0x4120: // FACE_ARRAY
                {
                    uint16_t numFaces = 0;
                    file.read(reinterpret_cast<char*>(&numFaces), sizeof(numFaces));

                    size_t startFaceIdx = mesh.faces.size();
                    mesh.faces.resize(startFaceIdx + numFaces);
                    mesh.materialForFace.resize(startFaceIdx + numFaces, 0);

                    std::vector<uint16_t> raw(size_t(numFaces) * 4);
                    file.read(reinterpret_cast<char*>(raw.data()), raw.size() * sizeof(uint16_t));

                    for (uint16_t i = 0; i < numFaces; ++i) {
                        mesh.faces[startFaceIdx + i].a = raw[i * 4 + 0] + vertexOffset;
                        mesh.faces[startFaceIdx + i].b = raw[i * 4 + 1] + vertexOffset;
                        mesh.faces[startFaceIdx + i].c = raw[i * 4 + 2] + vertexOffset;
                    }

                    // Parsuj pod-bloki FACE_ARRAY (np. FACE_MATERIAL 0x4130)
                    parseChunk(file, nextChunk, mesh, currentMaterial, vertexOffset);
                    break;
                }

                case 0x4130: // FACE_MATERIAL
                {
                    std::string materialName = readString(file, nextChunk);
                    uint16_t faceCount = 0;
                    file.read(reinterpret_cast<char*>(&faceCount), sizeof(faceCount));

                    int materialIndex = findMaterial(mesh, materialName);

                    if (materialIndex >= 0) {
                        for (uint16_t i = 0; i < faceCount; ++i) {
                            uint16_t faceIndex = 0;
                            file.read(reinterpret_cast<char*>(&faceIndex), sizeof(faceIndex));

                            // Zgodność z offsetem obecnego obiektu
                            size_t realFaceIdx = mesh.faces.size() > 0 ? (mesh.faces.size() - faceCount + i) : faceIndex;
                            if (realFaceIdx < mesh.materialForFace.size()) {
                                mesh.materialForFace[realFaceIdx] = static_cast<uint16_t>(materialIndex);
                            }
                        }
                    }
                    break;
                }

                case 0x4160: // TRI_LOCAL
                {
                    float m[12];
                    file.read(reinterpret_cast<char*>(m), sizeof(m));

                    mesh.localTransform = glm::mat4(1.0f);
                    mesh.localTransform[0][0] = m[0];  mesh.localTransform[0][1] = m[1];  mesh.localTransform[0][2] = m[2];
                    mesh.localTransform[1][0] = m[3];  mesh.localTransform[1][1] = m[4];  mesh.localTransform[1][2] = m[5];
                    mesh.localTransform[2][0] = m[6];  mesh.localTransform[2][1] = m[7];  mesh.localTransform[2][2] = m[8];
                    mesh.localTransform[3][0] = m[9];  mesh.localTransform[3][1] = m[10]; mesh.localTransform[3][2] = m[11];

                    mesh.hasLocalTransform = true;
                    break;
                }

                default:
                    break;
            }

            // GWARANCJA Przesunięcia na następny Chunk (zapobiega zapętleniu)
            file.seekg(nextChunk, std::ios::beg);
        }
    }
};