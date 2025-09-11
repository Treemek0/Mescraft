#pragma once
#include <vector>
#include <glad/glad.h>
#include <utility> // For std::exchange
#include <array>
#include <memory>
#include <unordered_map>
#include "../world/chunk.h"
#include <shared_mutex>

struct Vertex {
    glm::vec3 pos;      // 12 bytes
    glm::vec2 texCoord; // 8 bytes
    uint8_t light;      // 1 byte
    uint8_t pad[3];     // padding to align to 4 bytes boundary
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
};

struct Mesh {
    unsigned int VAO, VBO, EBO;
    size_t indexCount;

    glm::vec3 startPositonOfChunk; // for frustum

    MeshData data;
};

class MeshSystem {
public:
    const int atlasWidthPixels = 96;
    const int atlasHeightPixels = 1048;
    const int blockTexSize = 32;

    Mesh createMesh(MeshData& meshData);
    void deleteMesh(const Mesh& mesh);
    MeshData createChunkData(const Chunk& chunk, const std::array<std::shared_ptr<Chunk>, 6>& neighbors, int LOD = 1);
    void updateMeshDataWithBlock(Mesh& mesh, const Chunk& chunk, const std::unordered_map<uint64_t, std::shared_ptr<Chunk>>& chunkMap, int x, int y, int z);
    void updateChunkMesh(Mesh& chunkMesh);

private:
    void rotateUV(glm::vec2 uvs[4], int rotation);
};