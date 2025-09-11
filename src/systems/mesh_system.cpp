#include "mesh_system.h"
#include <block.h>
#include <iostream>
#include <cstring>
#include <world.h>
#include <array>
#include <memory>

Mesh MeshSystem::createMesh(MeshData& meshData) {
    Mesh mesh;

    glGenVertexArrays(1, &mesh.VAO);
    glBindVertexArray(mesh.VAO);

    glGenBuffers(1, &mesh.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, meshData.vertices.size() * sizeof(Vertex),
                 meshData.vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &mesh.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData.indices.size() * sizeof(unsigned int),
                 meshData.indices.data(), GL_STATIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);

    // TexCoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(1);

    // Light
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_BYTE, sizeof(Vertex), (void*)offsetof(Vertex, light));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    mesh.indexCount = meshData.indices.size();
    mesh.data = std::move(meshData);
    return mesh;
}

MeshData MeshSystem::createChunkData(const Chunk& chunk, const std::array<std::shared_ptr<Chunk>, 6>& neighbors, int LOD) {
    std::vector<Vertex> vertices;
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

    // corresponds to E, W, U, D, N, S
    const int neighbor_map[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };

    auto isAir = [&](int x, int y, int z) -> bool {
        int c_cx = chunk.position.x / CHUNK_SIZE;
        int c_cy = chunk.position.y / CHUNK_SIZE;
        int c_cz = chunk.position.z / CHUNK_SIZE;

        int n_cx = floor(static_cast<float>(x) / CHUNK_SIZE);
        int n_cy = floor(static_cast<float>(y) / CHUNK_SIZE);
        int n_cz = floor(static_cast<float>(z) / CHUNK_SIZE);

        if (n_cx == c_cx && n_cy == c_cy && n_cz == c_cz) {
            return getBlockID(chunk, getLocalCoord(x), getLocalCoord(y), getLocalCoord(z)) == 0;
        }

        int dx = n_cx - c_cx;
        int dy = n_cy - c_cy;
        int dz = n_cz - c_cz;

        for (int i = 0; i < 6; ++i) {
            if (dx == neighbor_map[i][0] && dy == neighbor_map[i][1] && dz == neighbor_map[i][2]) {
                const auto& neighbor_chunk = neighbors[i];
                if (neighbor_chunk) {
                    return getBlockID(*neighbor_chunk, getLocalCoord(x), getLocalCoord(y), getLocalCoord(z)) == 0;
                }
                break;
            }
        }

        return true;
    };

    for(int dz=0; dz<CHUNK_SIZE; dz++){
        for(int dy=0; dy<CHUNK_SIZE; dy++){
            for(int dx=0; dx<CHUNK_SIZE; dx++){
                const BlockData& blockData = chunk.blocks[dx + dy * CHUNK_SIZE + dz * CHUNK_SIZE * CHUNK_SIZE];
                uint8_t id = blockData.id;
                if (id == 0) continue; // skip air blocks

                int x = dx + chunk.position.x;
                int y = dy + chunk.position.y;
                int z = dz + chunk.position.z;

                for(int f=0; f<6; f++){
                    glm::ivec3 neighbor = glm::ivec3(x,y,z) + glm::ivec3(faceNormals[f]);
                    if(isAir(neighbor.x, neighbor.y, neighbor.z)){

                        // --- UV selection ---
                        int ty = (id - 1) * blockTexSize;
                        int tx;

                        // 0: Side, 1: Bottom, 2: Top
                        int faceTextureType = 0; // texture for side
                        switch (blockData.rotation) {
                            case 0: // normal rotation
                                if (f == 4) faceTextureType = 2; // Top face
                                if (f == 5) faceTextureType = 1; // Bottom face
                                break;
                            case 1: // -Z 
                            case 3: // +Z face
                                if (f == 0 || f == 1) faceTextureType = 2; // top texture
                                break;
                            case 2: // -X face
                            case 4: // +X face
                                if (f == 2 || f == 3) faceTextureType = 2; // bottom texture
                                break;
                        }

                        if (faceTextureType == 0) tx = 0;                  // Side
                        else if (faceTextureType == 1) tx = blockTexSize;     // Bottom
                        else if (faceTextureType == 2) tx = blockTexSize * 2; // Top

                        float u0 = float(tx) / atlasWidthPixels;
                        float v0 = float(ty) / atlasHeightPixels;
                        float u1 = float(tx + blockTexSize) / atlasWidthPixels;
                        float v1 = float(ty + blockTexSize) / atlasHeightPixels;

                        glm::vec2 uvs[4] = {
                            {u0, v1}, {u1, v1}, {u1, v0}, {u0, v0}
                        };

                        if (f == 2) { // -X face rotation for correct alaiwfuaaawdawdsgdyua
                            rotateUV(uvs, 90);
                        }

                        // 1  -Z rotation
                        // 2  -X rotation
                        // 3  +Z rotation
                        // 4  +X rotation

                        if ((blockData.rotation == 1 || blockData.rotation == 3) && (f == 2 || f == 3 || f == 5)) { // Z-aligned, top/bottom sides
                            rotateUV(uvs, 90); // Rotate 90 degrees
                        }
                        
                        if ((blockData.rotation == 2 || blockData.rotation == 4) && (f == 0 || f == 1 || f == 4)) { // X-aligned, front/back sides
                            rotateUV(uvs, 90);  // Rotate 90 degrees
                        }

                        uint8_t light = 255;
                        if(f < 4) light = 200;
                        if(f == 5) light = 150;


                        // Adds vertices
                        for (int vert = 0; vert < 4; ++vert) {
                            Vertex v;
                            v.pos = glm::vec3(x + faceVerts[f][vert].x, y + faceVerts[f][vert].y, z + faceVerts[f][vert].z);
                            v.texCoord = uvs[vert];
                            v.light = light;
                            vertices.push_back(v);
                        }

                        // Adds indices
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
    data.vertices = std::move(vertices);
    return data;
}

void MeshSystem::rotateUV(glm::vec2 uvs[4], int rotation){
    if(rotation == 90){
        std::swap(uvs[0], uvs[3]); std::swap(uvs[0], uvs[1]); std::swap(uvs[1], uvs[2]);
    }else if(rotation == -90){
        std::swap(uvs[0], uvs[1]); std::swap(uvs[0], uvs[3]); std::swap(uvs[2], uvs[3]);
    }
}

// maybe will use some day
// void MeshSystem::updateMeshDataWithBlock(Mesh& mesh, const Chunk& chunk, const std::unordered_map<uint64_t, std::shared_ptr<Chunk>>& chunkMap, int x, int y, int z) {
//     MeshData& meshData = mesh.data;

//     // This lambda should be refactored into a helper if used in multiple places.
//     auto isAir = [&](int gx, int gy, int gz) -> bool {
//         int cx = floor(gx / float(CHUNK_SIZE));
//         int cy = floor(gy / float(CHUNK_SIZE));
//         int cz = floor(gz / float(CHUNK_SIZE));

//         int lx = gx - cx * CHUNK_SIZE;
//         int ly = gy - cy * CHUNK_SIZE;
//         int lz = gz - cz * CHUNK_SIZE;

//         uint64_t key = hashChunkCoords(cx, cy, cz);
//         auto it = chunkMap.find(key);
//         if(it == chunkMap.end()) return true; // treat missing chunk as air

//         const auto& c_ptr = it->second;
//         return c_ptr->blocks[lx + ly*CHUNK_SIZE + lz*CHUNK_SIZE*CHUNK_SIZE].id == 0;
//     };

//     // Use the same face definitions as createChunkData for consistency
//     static const glm::vec3 faceVerts[6][4] = {
//         { {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0} }, // -Z
//         { {1,0,1}, {0,0,1}, {0,1,1}, {1,1,1} }, // +Z
//         { {0,0,0}, {0,1,0}, {0,1,1}, {0,0,1} }, // -X
//         { {1,0,0}, {1,0,1}, {1,1,1}, {1,1,0} }, // +X
//         { {0,1,0}, {1,1,0}, {1,1,1}, {0,1,1} }, // +Y
//         { {0,0,0}, {0,0,1}, {1,0,1}, {1,0,0} }  // -Y
//     };

//     static const glm::vec3 faceNormals[6] = {
//         {0,0,-1}, {0,0,1}, {-1,0,0}, {1,0,0}, {0,1,0}, {0,-1,0}
//     };

//     uint8_t id = chunk.blocks[x + y*CHUNK_SIZE + z*CHUNK_SIZE*CHUNK_SIZE];
//     if (id == 0) {
//         return;
//     }

//     unsigned int vertexOffset = meshData.vertices.size() / 6; // FIX: Stride is 6 floats per vertex

//     int gx = x + chunk.position.x;
//     int gy = y + chunk.position.y;
//     int gz = z + chunk.position.z;

//     for (int f = 0; f < 6; f++) {
//         glm::ivec3 neighborPos = glm::ivec3(gx, gy, gz) + glm::ivec3(faceNormals[f]);

//         if (isAir(neighborPos.x, neighborPos.y, neighborPos.z)) {
//             // --- UV selection ---
//             int ty = (id - 1) * blockTexSize; 
//             int tx;

//             if (f < 4)       tx = 0;                  // sides
//             else if (f == 4) tx = blockTexSize * 2;    // top
//             else             tx = blockTexSize;       // bottom

//             float u0 = float(tx) / atlasWidthPixels;
//             float v0 = float(ty) / atlasHeightPixels;
//             float u1 = float(tx + blockTexSize) / atlasWidthPixels;
//             float v1 = float(ty + blockTexSize) / atlasHeightPixels;

//             float faceU[4] = {u0, u1, u1, u0};
//             float faceV[4] = {v1, v1, v0, v0};

//             if (f == 2) { // -X face, same rotation as createChunkData
//                 float tmpU[4] = { faceU[1], faceU[2], faceU[3], faceU[0] };
//                 float tmpV[4] = { faceV[1], faceV[2], faceV[3], faceV[0] };
//                 memcpy(faceU, tmpU, sizeof(faceU));
//                 memcpy(faceV, tmpV, sizeof(faceV));
//             }

            
//             uint8_t light = 255;

//             for (int vert = 0; vert < 4; ++vert) {
//                 Vertex v;
//                 v.pos = glm::vec3(x + faceVerts[f][vert].x,
//                                 y + faceVerts[f][vert].y,
//                                 z + faceVerts[f][vert].z);
//                 v.texCoord = glm::vec2(faceU[vert], faceV[vert]);
//                 v.light = light;
//                 meshData.vertices.push_back(v);
//             }

//             meshData.indices.push_back(vertexOffset + 0);
//             meshData.indices.push_back(vertexOffset + 2);
//             meshData.indices.push_back(vertexOffset + 1);
//             meshData.indices.push_back(vertexOffset + 0);
//             meshData.indices.push_back(vertexOffset + 3);
//             meshData.indices.push_back(vertexOffset + 2);

//             vertexOffset += 4;
//         }
//     }

//     updateChunkMesh(mesh); // push updated vertices/indices to GPU
// }



// void MeshSystem::updateChunkMesh(Mesh& chunkMesh) {
//     glBindBuffer(GL_ARRAY_BUFFER, chunkMesh.VBO);
//     glBufferData(GL_ARRAY_BUFFER, chunkMesh.data.vertices.size() * sizeof(float), chunkMesh.data.vertices.data(), GL_STATIC_DRAW);

//     glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunkMesh.EBO);
//     glBufferData(GL_ELEMENT_ARRAY_BUFFER, chunkMesh.data.indices.size() * sizeof(unsigned int), chunkMesh.data.indices.data(), GL_STATIC_DRAW);

//     glBindBuffer(GL_ARRAY_BUFFER, 0);
//     glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

//     chunkMesh.indexCount = chunkMesh.data.indices.size();
// }


void MeshSystem::deleteMesh(const Mesh& mesh) {
    glDeleteBuffers(1, &mesh.VBO);
    glDeleteBuffers(1, &mesh.EBO);
    glDeleteVertexArrays(1, &mesh.VAO);
}