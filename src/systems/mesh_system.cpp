#include "mesh_system.h"
#include <block.h>
#include <iostream>
#include <cstring>
#include <world.h>

Mesh MeshSystem::createMesh(MeshData& meshData) {
    Mesh mesh;

    // VAO
    glGenVertexArrays(1, &mesh.VAO);
    glBindVertexArray(mesh.VAO);

    // VBO
    glGenBuffers(1, &mesh.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, meshData.vertices.size() * sizeof(float), meshData.vertices.data(), GL_STATIC_DRAW);

    // EBO
    glGenBuffers(1, &mesh.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData.indices.size() * sizeof(unsigned int), meshData.indices.data(), GL_STATIC_DRAW);

    // Vertex attributes (assume position=3 floats, texcoord=2 floats)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0); // unbind VAO

    mesh.indexCount = meshData.indices.size();
    mesh.data = std::move(meshData);

    return mesh;
}

void MeshSystem::updateChunkMesh(Mesh& chunkMesh) {
    glBindBuffer(GL_ARRAY_BUFFER, chunkMesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, chunkMesh.data.vertices.size() * sizeof(float), chunkMesh.data.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunkMesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, chunkMesh.data.indices.size() * sizeof(unsigned int), chunkMesh.data.indices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    chunkMesh.indexCount = chunkMesh.data.indices.size();
}

MeshData MeshSystem::createChunkData(const Chunk& chunk, std::unordered_map<uint64_t, Chunk>& chunkMap) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int indexOffset = 0;

    glm::vec3 faceVerts[6][4] = {
        // -Z (front)
        { {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0} },
        // +Z (back)
        { {1,0,1}, {0,0,1}, {0,1,1}, {1,1,1} },
        // -X (left)
        { {0,0,0}, {0,1,0}, {0,1,1}, {0,0,1} },
        // +X (right)
        { {1,0,0}, {1,0,1}, {1,1,1}, {1,1,0} },
        // +Y (top)
        { {0,1,0}, {1,1,0}, {1,1,1}, {0,1,1} },
        // -Y (bottom)
        { {0,0,0}, {0,0,1}, {1,0,1}, {1,0,0} }
    };

    glm::vec3 faceNormals[6] = {
        {0,0,-1}, {0,0,1}, {-1,0,0}, {1,0,0}, {0,1,0}, {0,-1,0}
    };

    auto isAir = [&](int x, int y, int z) -> bool {
        int cx = floor(x / float(CHUNK_SIZE));
        int cy = floor(y / float(CHUNK_SIZE));
        int cz = floor(z / float(CHUNK_SIZE));

        int lx = x - cx * CHUNK_SIZE;
        int ly = y - cy * CHUNK_SIZE;
        int lz = z - cz * CHUNK_SIZE;

        uint64_t key = hashChunkCoords(cx, cy, cz);
        auto it = chunkMap.find(key);
        if(it == chunkMap.end()) return true; // treat missing chunk as air

        const Chunk& c = it->second;
        return c.blocks[lx + ly*CHUNK_SIZE + lz*CHUNK_SIZE*CHUNK_SIZE] == 0;
    };

    for(int dz=0; dz<CHUNK_SIZE; dz++){
        for(int dy=0; dy<CHUNK_SIZE; dy++){
            for(int dx=0; dx<CHUNK_SIZE; dx++){
                uint8_t id = chunk.blocks[dx + dy*CHUNK_SIZE + dz*CHUNK_SIZE*CHUNK_SIZE];
                if (id == 0) continue; // skip air

                int x = dx + chunk.position.x;
                int y = dy + chunk.position.y;
                int z = dz + chunk.position.z;
                
                

                for(int f=0; f<6; f++){
                    glm::ivec3 neighbor = glm::ivec3(x,y,z) + glm::ivec3(faceNormals[f]);
                    if(isAir(neighbor.x, neighbor.y, neighbor.z)){

                        // --- UV selection ---
                        int ty = (id - 1) * blockTexSize; 
                        int tx;

                        if (f < 4)         tx = 0;                  // sides
                        else if (f == 4)   tx = blockTexSize * 2;        // top
                        else               tx = blockTexSize;    // bottom

                        float u0 = float(tx) / atlasWidthPixels;
                        float v0 = float(ty) / atlasHeightPixels;
                        float u1 = float(tx + blockTexSize) / atlasWidthPixels;
                        float v1 = float(ty + blockTexSize) / atlasHeightPixels;

                        float faceU[4] = { u0, u1, u1, u0 };
                        float faceV[4] = { v1, v1, v0, v0 };

                        if(f == 2){ // -X face
                            float tmpU[4] = { faceU[1], faceU[2], faceU[3], faceU[0] };
                            float tmpV[4] = { faceV[1], faceV[2], faceV[3], faceV[0] };
                            memcpy(faceU, tmpU, sizeof(faceU));
                            memcpy(faceV, tmpV, sizeof(faceV));
                        }

                        // Add vertices
                        for (int vert = 0; vert < 4; ++vert) {
                            vertices.push_back(x + faceVerts[f][vert].x);
                            vertices.push_back(y + faceVerts[f][vert].y);
                            vertices.push_back(z + faceVerts[f][vert].z);
                            vertices.push_back(faceU[vert]);
                            vertices.push_back(faceV[vert]);
                            vertices.push_back(f);
                        }

                        // Indices same as before
                        indices.push_back(indexOffset + 0);
                        indices.push_back(indexOffset + 2);
                        indices.push_back(indexOffset + 1);
                        indices.push_back(indexOffset + 0);
                        indices.push_back(indexOffset + 3);
                        indices.push_back(indexOffset + 2);

                        indexOffset += 4;
                    }
                }
            }
        }
    }

    MeshData data;
    data.indices = indices;
    data.vertices = vertices;
    return data;
}

// maybe will use some day
void MeshSystem::updateMeshDataWithBlock(Mesh& mesh, const Chunk& chunk, std::unordered_map<uint64_t, Chunk>& chunkMap, int x, int y, int z) {
    MeshData& meshData = mesh.data;

    // This lambda should be refactored into a helper if used in multiple places.
    auto isAir = [&](int gx, int gy, int gz) -> bool {
        int cx = floor(gx / float(CHUNK_SIZE));
        int cy = floor(gy / float(CHUNK_SIZE));
        int cz = floor(gz / float(CHUNK_SIZE));

        int lx = gx - cx * CHUNK_SIZE;
        int ly = gy - cy * CHUNK_SIZE;
        int lz = gz - cz * CHUNK_SIZE;

        uint64_t key = hashChunkCoords(cx, cy, cz);
        auto it = chunkMap.find(key);
        if(it == chunkMap.end()) return true; // treat missing chunk as air

        const Chunk& c = it->second;
        return c.blocks[lx + ly*CHUNK_SIZE + lz*CHUNK_SIZE*CHUNK_SIZE] == 0;
    };

    // Use the same face definitions as createChunkData for consistency
    static const glm::vec3 faceVerts[6][4] = {
        { {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0} }, // -Z
        { {1,0,1}, {0,0,1}, {0,1,1}, {1,1,1} }, // +Z
        { {0,0,0}, {0,1,0}, {0,1,1}, {0,0,1} }, // -X
        { {1,0,0}, {1,0,1}, {1,1,1}, {1,1,0} }, // +X
        { {0,1,0}, {1,1,0}, {1,1,1}, {0,1,1} }, // +Y
        { {0,0,0}, {0,0,1}, {1,0,1}, {1,0,0} }  // -Y
    };

    static const glm::vec3 faceNormals[6] = {
        {0,0,-1}, {0,0,1}, {-1,0,0}, {1,0,0}, {0,1,0}, {0,-1,0}
    };

    uint8_t id = chunk.blocks[x + y*CHUNK_SIZE + z*CHUNK_SIZE*CHUNK_SIZE];
    if (id == 0) {
        return;
    }

    unsigned int vertexOffset = meshData.vertices.size() / 6; // FIX: Stride is 6 floats per vertex

    int gx = x + chunk.position.x;
    int gy = y + chunk.position.y;
    int gz = z + chunk.position.z;

    for (int f = 0; f < 6; f++) {
        glm::ivec3 neighborPos = glm::ivec3(gx, gy, gz) + glm::ivec3(faceNormals[f]);

        if (isAir(neighborPos.x, neighborPos.y, neighborPos.z)) {
            // --- UV selection ---
            int ty = (id - 1) * blockTexSize; 
            int tx;

            if (f < 4)       tx = 0;                  // sides
            else if (f == 4) tx = blockTexSize * 2;    // top
            else             tx = blockTexSize;       // bottom

            float u0 = float(tx) / atlasWidthPixels;
            float v0 = float(ty) / atlasHeightPixels;
            float u1 = float(tx + blockTexSize) / atlasWidthPixels;
            float v1 = float(ty + blockTexSize) / atlasHeightPixels;

            float faceU[4] = {u0, u1, u1, u0};
            float faceV[4] = {v1, v1, v0, v0};

            if (f == 2) { // -X face, same rotation as createChunkData
                float tmpU[4] = { faceU[1], faceU[2], faceU[3], faceU[0] };
                float tmpV[4] = { faceV[1], faceV[2], faceV[3], faceV[0] };
                memcpy(faceU, tmpU, sizeof(faceU));
                memcpy(faceV, tmpV, sizeof(faceV));
            }

            
            for (int vert = 0; vert < 4; ++vert) {
                meshData.vertices.push_back(gx + faceVerts[f][vert].x);
                meshData.vertices.push_back(gy + faceVerts[f][vert].y);
                meshData.vertices.push_back(gz + faceVerts[f][vert].z);
                meshData.vertices.push_back(faceU[vert]);
                meshData.vertices.push_back(faceV[vert]);
                meshData.vertices.push_back(f);
            }

            meshData.indices.push_back(vertexOffset + 0);
            meshData.indices.push_back(vertexOffset + 2);
            meshData.indices.push_back(vertexOffset + 1);
            meshData.indices.push_back(vertexOffset + 0);
            meshData.indices.push_back(vertexOffset + 3);
            meshData.indices.push_back(vertexOffset + 2);

            vertexOffset += 4;
        }
    }

    updateChunkMesh(mesh); // push updated vertices/indices to GPU
}

void MeshSystem::deleteMesh(const Mesh& mesh) {
    glDeleteBuffers(1, &mesh.VBO);
    glDeleteBuffers(1, &mesh.EBO);
    glDeleteVertexArrays(1, &mesh.VAO);
}