#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "./block.h"
#include "../config.h"
#include <iostream>
#include <fstream>
#include <filesystem>

struct Chunk {
    glm::vec3 position;
    
    std::unordered_map<uint64_t, BlockData> modifiedBlockMap;
    BlockData blocks[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE] = {}; // 8x8x8 blocks, each block ID is 1 byte (0-255)
};

inline uint64_t hashChunkCoords(int x, int y, int z) {
    uint64_t ux = (uint64_t)(x + 1000000) & 0x1FFFFF; // 21 bits
    uint64_t uy = (uint64_t)(y + 1000000) & 0x1FFFFF; // 21 bits
    uint64_t uz = (uint64_t)(z + 1000000) & 0x1FFFFF; // 21 bits

    return (ux << 42) | (uy << 21) | uz;
}

inline uint8_t getBlockID(const Chunk& chunk, int x, int y, int z) {
    return chunk.blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE].id; // x + y*chunkSize + z*chunkSize*chunkSize; (0-7 coords)
}

inline int getLocalCoord(int worldCoord) {
    int coord = worldCoord % CHUNK_SIZE;
    if (coord < 0) coord += CHUNK_SIZE; // fix negatives
    return coord;
}

inline void setBlockID(Chunk& chunk, int x, int y, int z, uint8_t id) {
    chunk.blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE].id = id;
}

inline void changeBlockID(Chunk& chunk, int x, int y, int z, uint8_t id) {
    chunk.blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE].id = id;

    uint64_t key = hashChunkCoords(x, y, z);
    chunk.modifiedBlockMap[key].id = id;
}

inline void changeBlockRotation(Chunk& chunk, int x, int y, int z, uint8_t rotation) {
    chunk.blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE].rotation= rotation;

    uint64_t key = hashChunkCoords(x, y, z);
    chunk.modifiedBlockMap[key].rotation= rotation;
}

inline void decodeChunkHash(uint64_t hash, int &x, int &y, int &z) {
    uint64_t mask = 0x1FFFFF; // 21 bits

    uint64_t ux = (hash >> 42) & mask;
    uint64_t uy = (hash >> 21) & mask;
    uint64_t uz = hash & mask;

    x = static_cast<int>(ux) - 1000000;
    y = static_cast<int>(uy) - 1000000;
    z = static_cast<int>(uz) - 1000000;
}

inline int getChunkHashFromWorldCoords(int x, int y, int z) {
    return hashChunkCoords(floor(x / CHUNK_SIZE), floor(y / CHUNK_SIZE), floor(z / CHUNK_SIZE));
}


inline void writeChunkToFile(const Chunk& chunk, int seed) {
    if (chunk.modifiedBlockMap.empty()) return;

    std::string CHUNK_FOLDER = "worlds/" + std::to_string(seed);

    try {
        if (!std::filesystem::exists(CHUNK_FOLDER)) {
            std::filesystem::create_directories(CHUNK_FOLDER);
            std::cout << "Created directory: " << CHUNK_FOLDER << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error creating directory: " << e.what() << std::endl;
        return;
    }

    std::string filename = std::to_string(chunk.position.x / CHUNK_SIZE) + "_" + std::to_string(chunk.position.y / CHUNK_SIZE) + "_" + std::to_string(chunk.position.z / CHUNK_SIZE) + ".chunk";
    
    std::ofstream outputFile(CHUNK_FOLDER + "/" + filename, std::ios::out | std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "Error: Could not open file for writing." << std::endl;
        return;
    }

    size_t numBlocks = chunk.modifiedBlockMap.size();
    outputFile.write(reinterpret_cast<const char*>(&numBlocks), sizeof(numBlocks));

    for (const auto& pair : chunk.modifiedBlockMap) {
        outputFile.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
        outputFile.write(reinterpret_cast<const char*>(&pair.second.id), sizeof(pair.second.id));
        outputFile.write(reinterpret_cast<const char*>(&pair.second.rotation), sizeof(pair.second.rotation));
    }

    outputFile.close();
    std::cout << "Saved " << numBlocks << " modified blocks." << std::endl;
}

inline bool readChunkFromFile(Chunk& chunk, int seed) {
    std::string CHUNK_FOLDER = "worlds/" + std::to_string(seed);
    std::string filename = std::to_string(chunk.position.x / CHUNK_SIZE) + "_" + std::to_string(chunk.position.y / CHUNK_SIZE) + "_" + std::to_string(chunk.position.z / CHUNK_SIZE) + ".chunk";
    std::string fullPath = CHUNK_FOLDER + "/" + filename;

    std::ifstream inputFile(fullPath, std::ios::in | std::ios::binary);
    if (!inputFile.is_open()) return false;

    try {
        size_t numBlocks = 0;
        inputFile.read(reinterpret_cast<char*>(&numBlocks), sizeof(numBlocks));

        // Quick sanity check
        if (numBlocks > CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE) {
            std::cerr << "Chunk file is corrupted (too many blocks): " << fullPath << std::endl;
            return false;
        }

        for (size_t i = 0; i < numBlocks; ++i) {
            uint64_t key = 0;
            BlockData data{};
            
            // Check that reading does not fail
            if (!inputFile.read(reinterpret_cast<char*>(&key), sizeof(key)) ||
                !inputFile.read(reinterpret_cast<char*>(&data.id), sizeof(data.id)) ||
                !inputFile.read(reinterpret_cast<char*>(&data.rotation), sizeof(data.rotation))) 
            {
                std::cerr << "Chunk file read error or corrupted: " << fullPath << std::endl;
                return false;
            }

            // Basic validation
            if (data.id > 255) {
                std::cerr << "Invalid block data in file: " << fullPath << std::endl;
                continue; // skip corrupted block
            }

            chunk.modifiedBlockMap[key] = data;

            int x, y, z;
            decodeChunkHash(key, x, y, z);

            // Validate coordinates inside chunk bounds
            if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
                std::cerr << "Block coordinates out of bounds: " << key << std::endl;
                continue;
            }

            chunk.blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE] = data;
        }

        inputFile.close();
        std::cout << "Successfully read " << chunk.modifiedBlockMap.size() << " modified blocks from " << fullPath << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception reading chunk file " << fullPath << ": " << e.what() << std::endl;
        return false;
    }
}