#pragma once
#include <vector>
#include <glad/glad.h>
#include <utility> // For std::exchange
#include <unordered_map>
#include "../world/chunk.h"

struct MeshData {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};

struct Mesh {
    unsigned int VAO, VBO, EBO;
    size_t indexCount;

    MeshData data;
};

class MeshSystem {
public:
    const int atlasWidthPixels = 96;
    const int atlasHeightPixels = 1048;
    const int blockTexSize = 32;

    Mesh createMesh(MeshData& meshData);
    void deleteMesh(const Mesh& mesh);
    MeshData createChunkData(const Chunk& chunk, std::unordered_map<uint64_t, Chunk>& chunkMap);
    void updateMeshDataWithBlock(Mesh& mesh, const Chunk& chunk, std::unordered_map<uint64_t, Chunk>& chunkMap, int x, int y, int z);
    void updateChunkMesh(Mesh& chunkMesh);

};