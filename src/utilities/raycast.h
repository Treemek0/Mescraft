#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../world/chunk.h"
#include <memory>

struct RaycastHit {
    bool hit = false;
    int distance = 0;
    uint64_t chunkHash;
    Block block;       // world coords of block hit
    glm::ivec3 faceNormal;  // face normal (-1/0/1 per axis)
};

inline RaycastHit raycast( const glm::vec3& origin, const glm::vec3& dir, float maxDist, const std::unordered_map<uint64_t, std::shared_ptr<Chunk>>& chunkMap) {
    RaycastHit result;
    glm::vec3 rayDir = glm::normalize(dir);
    glm::ivec3 blockPos = glm::floor(origin);

    glm::vec3 offset = glm::step(glm::vec3(0.0f), rayDir); // 1 if >0
    glm::vec3 tMax = (glm::vec3(blockPos) + offset - origin) / rayDir;
    glm::vec3 tDelta = glm::abs(1.0f / rayDir);
    
    glm::ivec3 step;
    step.x = (rayDir.x > 0) ? 1 : -1;
    step.y = (rayDir.y > 0) ? 1 : -1;
    step.z = (rayDir.z > 0) ? 1 : -1;

    float t = 0.0f;
    int lastAxis = -1;

    while (t < maxDist) {
        if (t >= maxDist) break;

        // Floored division for chunk coordinates, correct for negative values
        int cx = blockPos.x / CHUNK_SIZE;
        if (blockPos.x < 0 && blockPos.x % CHUNK_SIZE != 0) --cx;
        int cy = blockPos.y / CHUNK_SIZE;
        if (blockPos.y < 0 && blockPos.y % CHUNK_SIZE != 0) --cy;
        int cz = blockPos.z / CHUNK_SIZE;
        if (blockPos.z < 0 && blockPos.z % CHUNK_SIZE != 0) --cz;

        uint64_t hash = hashChunkCoords(cx, cy, cz);

        auto it = chunkMap.find(hash);
        if (it != chunkMap.end()) {
            const Chunk& chunk = *(it->second);
            int lx = getLocalCoord(blockPos.x);
            int ly = getLocalCoord(blockPos.y);
            int lz = getLocalCoord(blockPos.z);

            uint8_t id = getBlockID(chunk, lx, ly, lz);
            if (id != 0) {
                Block block;
                block.data.id = id;
                block.position = blockPos;

                result.block = block;
                result.chunkHash = hash;
                result.hit = true;

                if (lastAxis == 0) result.faceNormal = { -step.x, 0, 0 };
                if (lastAxis == 1) result.faceNormal = { 0, -step.y, 0 };
                if (lastAxis == 2) result.faceNormal = { 0, 0, -step.z };

                result.distance = t;

                return result;
            }
        }

                // advance ray to the next block boundary
        if (tMax.x < tMax.y && tMax.x < tMax.z) {
            blockPos.x += step.x;
            t = tMax.x;
            tMax.x += tDelta.x;
            lastAxis = 0;
        } else if (tMax.y < tMax.z) {
            blockPos.y += step.y;
            t = tMax.y;
            tMax.y += tDelta.y;
            lastAxis = 1;
        } else {
            blockPos.z += step.z;
            t = tMax.z;
            tMax.z += tDelta.z;
            lastAxis = 2;
        }
    }

    return result;
}