#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>

struct Vertex {
    float x, y, z;
};

struct Face {
    uint16_t a, b, c;
};

struct Mesh3DS {
    std::vector<Vertex> vertices;
    std::vector<Face> faces;
};

class Loader3DS {
public:
    static bool load(const std::string& filepath, Mesh3DS& outMesh) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) return false;

        parseChunk(file, getFileSize(file), outMesh);
        return !outMesh.vertices.empty();
    }

private:
    static std::streampos getFileSize(std::ifstream& file) {
        file.seekg(0, std::ios::end);
        std::streampos size = file.tellg();
        file.seekg(0, std::ios::beg);
        return size;
    }

    static void parseChunk(std::ifstream& file, std::uint32_t endPos, Mesh3DS& mesh) {
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
                    parseChunk(file, nextChunk, mesh);
                    break;
                }
                case 0x4100: // N_TRI_OBJECT (MeshData)
                    parseChunk(file, nextChunk, mesh);
                    break;

                case 0x4110: // POINT_ARRAY (Wierzchołki)
                {
                    uint16_t numVertices;
                    file.read(reinterpret_cast<char*>(&numVertices), sizeof(numVertices));
                    mesh.vertices.resize(numVertices);
                    file.read(reinterpret_cast<char*>(mesh.vertices.data()), numVertices * sizeof(Vertex));
                    break;
                }
                case 0x4120: // FACE_ARRAY (Trójkąty/Indeksy)
                {
                    uint16_t numFaces;
                    file.read(reinterpret_cast<char*>(&numFaces), sizeof(numFaces));
                    mesh.faces.resize(numFaces);
                    for (int i = 0; i < numFaces; ++i) {
                        file.read(reinterpret_cast<char*>(&mesh.faces[i]), 3 * sizeof(uint16_t));
                        uint16_t flags; // 3DS zapisuje dodatkową flagę po każdym ścianie
                        file.read(reinterpret_cast<char*>(&flags), sizeof(flags));
                    }
                    break;
                }
                default:
                    file.seekg(nextChunk, std::ios::beg);
                    break;
            }
        }
    }
};