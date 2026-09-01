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

#include <zenkit/vobs/VirtualObject.hh>

namespace fs = std::filesystem;

struct Mesh3DSVertex
{
    glm::vec3 pos;
    glm::vec2 uv;
    glm::vec3 normal;
};

struct Mesh3DSFace
{
    uint16_t a, b, c;
};



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

    // Materiały znalezione w pliku 3DS
    std::vector<Mesh3DSMaterial> materials;

    // Dla każdego face'a: indeks materiału.
    // materialForFace[i] odpowiada faces[i].
    std::vector<uint16_t> materialForFace;

    glm::vec3 minBounds{0.0f};
    glm::vec3 maxBounds{0.0f};
    glm::vec3 center{0.0f};
    float maxDimension = 1.0f;

    glm::mat4 localTransform{1.0f};
    bool hasLocalTransform = false;
};


struct GPUMeshPart
{
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;

    GLsizei indexCount = 0;

    glm::vec3 minBounds{0.0f};
    glm::vec3 maxBounds{0.0f};

    GLuint texture = 0;

    glm::vec3 fallbackColor{0.6f, 0.6f, 0.62f};
};


struct GPUMesh
{
    std::vector<GPUMeshPart> parts;

    glm::vec3 minBounds;
    glm::vec3 maxBounds;
};


struct RenderableVob
{
    std::shared_ptr<zenkit::VirtualObject> vob;
    std::shared_ptr<GPUMeshPart> mesh;
};


class Loader3DS {
public:
    static bool load(const std::string& filepath, Mesh3DS& outMesh) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open())
            return false;

        parseChunk(file, getFileSize(file), outMesh);

        if (outMesh.vertices.empty())
            return false;

        glm::vec3 minB(
            outMesh.vertices[0].x,
            outMesh.vertices[0].y,
            outMesh.vertices[0].z
        );

        glm::vec3 maxB = minB;

        for (const auto& v : outMesh.vertices)
        {
            glm::vec3 p(v.x, v.y, v.z);

            minB = glm::min(minB, p);
            maxB = glm::max(maxB, p);
        }

        outMesh.minBounds = minB;
        outMesh.maxBounds = maxB;

        outMesh.center = (minB + maxB) * 0.5f;

        glm::vec3 size = maxB - minB;

        outMesh.maxDimension =
            std::max(
                size.x,
                std::max(size.y, size.z)
            );

        return true;
    }


private:
    static std::streampos getFileSize(std::ifstream& file) {
        file.seekg(0, std::ios::end);
        std::streampos size = file.tellg();
        file.seekg(0, std::ios::beg);
        return size;
    }

    static int createMaterial(Mesh3DS& mesh)
    {
        mesh.materials.push_back({});
        return static_cast<int>(mesh.materials.size() - 1);
    }

    static int findMaterial(
    const Mesh3DS& mesh,
    const std::string& name)
    {
        for (size_t i = 0; i < mesh.materials.size(); ++i)
        {
            if (mesh.materials[i].name == name)
                return static_cast<int>(i);
        }

        return -1;
    }


    static void parseChunk(
        std::ifstream& file,
        std::uint32_t endPos,
        Mesh3DS& mesh,
        int currentMaterial = -1)
    {
        while (file.tellg() < endPos && file.good()) {
            uint16_t chunkId;
            uint32_t chunkLength;

            file.read(reinterpret_cast<char*>(&chunkId), sizeof(chunkId));
            file.read(reinterpret_cast<char*>(&chunkLength), sizeof(chunkLength));

            std::uint32_t nextChunk = static_cast<std::uint32_t>(file.tellg()) + chunkLength - 6;

            switch (chunkId) {
                case 0x4D4D: // MAIN3DS
                case 0x3D3D: // EDIT3DS
                case 0x4000: // TRI_OBJECT
                {
                    if (chunkId == 0x4000) {
                        // Pomijamy nazwę obiektu zakończoną \0
                        char ch;
                        while (file.get(ch) && ch != '\0');
                    }
                    parseChunk(file, nextChunk, mesh, currentMaterial);

                    break;
                }
                case 0xA000: // MAT_NAME
                {
                    if (currentMaterial >= 0 &&
                        currentMaterial < static_cast<int>(mesh.materials.size()))
                    {
                        std::string materialName;
                        char ch;

                        while (file.tellg() < nextChunk &&
                            file.get(ch) &&
                            ch != '\0')
                        {
                            materialName += ch;
                        }

                        mesh.materials[currentMaterial].name = materialName;

                        printf(
                            "3DS MATERIAL NAME: '%s'\n",
                            materialName.c_str()
                        );
                    }

                    file.seekg(nextChunk, std::ios::beg);
                    break;
                }

                case 0x4100: // N_TRI_OBJECT (MeshData)
                    parseChunk(file, nextChunk, mesh, currentMaterial);

                    break;
                case 0xAFFF: // MATERIAL_BLOCK
                {
                    printf("3DS MATERIAL CHUNK\n");

                    int materialIndex = createMaterial(mesh);

                    parseChunk(
                        file,
                        nextChunk,
                        mesh,
                        materialIndex
                    );

                    break;
                }

                case 0xA200: // MAT_TEXMAP
                {
                    printf("3DS TEXTURE MAP CHUNK\n");

                    parseChunk(
                        file,
                        nextChunk,
                        mesh,
                        currentMaterial
                    );

                    break;
                }


                case 0xA300: // MAT_MAPNAME
                {
                    std::string textureName;

                    char ch;

                    while (file.tellg() < nextChunk &&
                        file.get(ch) &&
                        ch != '\0')
                    {
                        textureName += ch;
                    }

                    if (currentMaterial >= 0 &&
                        currentMaterial < static_cast<int>(mesh.materials.size()))
                    {
                        mesh.materials[currentMaterial].textureFile =
                            textureName;

                        printf(
                            "3DS TEXTURE FOUND: material=%d '%s'\n",
                            currentMaterial,
                            textureName.c_str()
                        );
                    }

                    file.seekg(nextChunk, std::ios::beg);

                    break;
                }


                case 0x4110: // POINT_ARRAY (Wierzchołki)
                {
                    uint16_t numVertices;
                    file.read(reinterpret_cast<char*>(&numVertices), sizeof(numVertices));
                    mesh.vertices.resize(numVertices);
                    file.read(reinterpret_cast<char*>(mesh.vertices.data()), numVertices * sizeof(VertexLoader));
                    break;
                }
                case 0x4130: // FACE_MATERIAL
                {
                    std::string materialName;

                    char ch;

                    while (file.tellg() < nextChunk &&
                        file.get(ch) &&
                        ch != '\0')
                    {
                        materialName += ch;
                    }

                    uint16_t faceCount = 0;

                    file.read(
                        reinterpret_cast<char*>(&faceCount),
                        sizeof(faceCount)
                    );

                    int materialIndex =
                        findMaterial(mesh, materialName);

                    printf(
                        "3DS FACE MATERIAL: '%s' -> material=%d, faces=%u\n",
                        materialName.c_str(),
                        materialIndex,
                        faceCount
                    );

                    if (materialIndex >= 0)
                    {
                        for (uint16_t i = 0; i < faceCount; ++i)
                        {
                            uint16_t faceIndex;

                            file.read(
                                reinterpret_cast<char*>(&faceIndex),
                                sizeof(faceIndex)
                            );

                            if (faceIndex < mesh.materialForFace.size())
                            {
                                mesh.materialForFace[faceIndex] =
                                    static_cast<uint16_t>(materialIndex);
                            }
                        }
                    }

                    file.seekg(nextChunk, std::ios::beg);


                    break;
                }


                case 0x4120: // FACE_ARRAY
                {
                    uint16_t numFaces;

                    file.read(
                        reinterpret_cast<char*>(&numFaces),
                        sizeof(numFaces)
                    );

                    mesh.faces.resize(numFaces);

                    mesh.materialForFace.resize(
                        numFaces,
                        0
                    );

                    for (uint16_t i = 0; i < numFaces; ++i)
                    {
                        file.read(
                            reinterpret_cast<char*>(&mesh.faces[i].a),
                            sizeof(uint16_t)
                        );

                        file.read(
                            reinterpret_cast<char*>(&mesh.faces[i].b),
                            sizeof(uint16_t)
                        );

                        file.read(
                            reinterpret_cast<char*>(&mesh.faces[i].c),
                            sizeof(uint16_t)
                        );

                        uint16_t flags;

                        file.read(
                            reinterpret_cast<char*>(&flags),
                            sizeof(uint16_t)
                        );
                    }

                    // UWAGA:
                    // tutaj nie czytamy jeszcze smoothing groups.
                    // Po FACE_ARRAY będą kolejne podchunki, m.in. 0x4150.

                    parseChunk(
                        file,
                        nextChunk,
                        mesh,
                        currentMaterial
                    );

                    break;
                }

                case 0x4160: // TRI_LOCAL
                {
                    float m[12];

                    file.read(
                        reinterpret_cast<char*>(m),
                        sizeof(m)
                    );

                    /*
                    3DS TRI_LOCAL:

                    m[0..2]   X axis
                    m[3..5]   Y axis
                    m[6..8]   Z axis
                    m[9..11]  origin

                    Macierz:
                    
                    | Xx Yx Zx Ox |
                    | Xy Yy Zy Oy |
                    | Xz Yz Zz Oz |
                    |  0  0  0  1 |
                    */

                    mesh.localTransform = glm::mat4(1.0f);

                    mesh.localTransform[0][0] = m[0];
                    mesh.localTransform[0][1] = m[1];
                    mesh.localTransform[0][2] = m[2];

                    mesh.localTransform[1][0] = m[3];
                    mesh.localTransform[1][1] = m[4];
                    mesh.localTransform[1][2] = m[5];

                    mesh.localTransform[2][0] = m[6];
                    mesh.localTransform[2][1] = m[7];
                    mesh.localTransform[2][2] = m[8];

                    mesh.localTransform[3][0] = m[9];
                    mesh.localTransform[3][1] = m[10];
                    mesh.localTransform[3][2] = m[11];

                    mesh.hasLocalTransform = true;

                    printf(
                        "3DS LOCAL MATRIX:\n"
                        "[ %.4f %.4f %.4f | %.4f ]\n"
                        "[ %.4f %.4f %.4f | %.4f ]\n"
                        "[ %.4f %.4f %.4f | %.4f ]\n",
                        m[0], m[3], m[6], m[9],
                        m[1], m[4], m[7], m[10],
                        m[2], m[5], m[8], m[11]
                    );

                    break;
                }

          
                default:
                {
                    // printf(
                    //     "3DS UNKNOWN CHUNK: 0x%04X, length=%u\n",
                    //     chunkId,
                    //     chunkLength
                    // );

                    file.seekg(nextChunk, std::ios::beg);
                    break;
                }

            }
        }
    }
};

std::string resolveMeshPath(
    const std::string& meshName,
    const std::string& gothicDir)
{
    fs::path meshes = fs::path(gothicDir) / "_Work" / "Data" / "Meshes";

    if (!fs::exists(meshes))
        return {};

    std::string wanted = meshName;
    std::transform(
        wanted.begin(),
        wanted.end(),
        wanted.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

    for (const auto& entry :
         fs::recursive_directory_iterator(meshes))
    {
        if (!entry.is_regular_file())
            continue;

        std::string filename =
            entry.path().filename().string();

        std::transform(
            filename.begin(),
            filename.end(),
            filename.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

        if (filename == wanted)
            return entry.path().string();
    }

    return {};
}

std::shared_ptr<GPUMeshPart> uploadMesh(
    const Mesh3DS& mesh)
{
    auto gpu = std::make_shared<GPUMeshPart>();

    glGenVertexArrays(1, &gpu->vao);
    glGenBuffers(1, &gpu->vbo);
    glGenBuffers(1, &gpu->ebo);

    glBindVertexArray(gpu->vao);

    // VBO
    glBindBuffer(GL_ARRAY_BUFFER, gpu->vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        mesh.vertices.size() * sizeof(VertexLoader),
        mesh.vertices.data(),
        GL_STATIC_DRAW
    );

    // EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu->ebo);

    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        mesh.faces.size() * sizeof(Face),
        mesh.faces.data(),
        GL_STATIC_DRAW
    );

    // POSITION
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(VertexLoader),
        (void*)offsetof(VertexLoader, x)
    );

    glEnableVertexAttribArray(0);

    gpu->indexCount =
        static_cast<GLsizei>(mesh.faces.size() * 3);

    // BOUNDS — zostawiamy, bo renderer może ich używać
    gpu->minBounds = mesh.minBounds;
    gpu->maxBounds = mesh.maxBounds;

    glBindVertexArray(0);

    return gpu;
}
