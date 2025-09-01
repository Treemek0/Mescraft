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
    
    std::unordered_map<uint64_t, uint8_t> modifiedBlockMap;
    uint8_t blocks[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE] = {}; // 8x8x8 blocks, each block ID is 1 byte (0-255)
};

inline uint64_t hashChunkCoords(int x, int y, int z) {
    uint64_t ux = (uint64_t)(x + 1000000) & 0x1FFFFF; // 21 bits
    uint64_t uy = (uint64_t)(y + 1000000) & 0x1FFFFF; // 21 bits
    uint64_t uz = (uint64_t)(z + 1000000) & 0x1FFFFF; // 21 bits

    return (ux << 42) | (uy << 21) | uz;
}

inline uint8_t getBlockID(const Chunk& chunk, int x, int y, int z) {
    return chunk.blocks[x + y*CHUNK_SIZE + z*CHUNK_SIZE*CHUNK_SIZE]; // x + y*chunkSize + z*chunkSize*chunkSize; (0-7 coords)
}

inline int getLocalCoord(int worldCoord) {
    int coord = worldCoord % CHUNK_SIZE;
    if (coord < 0) coord += CHUNK_SIZE; // fix negatives
    return coord;
}

inline void setBlockID(Chunk& chunk, int x, int y, int z, uint8_t id) {
    chunk.blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE] = id;
}

inline void changeBlockID(Chunk& chunk, int x, int y, int z, uint8_t id) {
    chunk.blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE] = id;

    uint64_t key = hashChunkCoords(x, y, z);
    chunk.modifiedBlockMap[key] = id;
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


inline void writeChunkToFile(const Chunk& chunk, int seed){
    if(chunk.modifiedBlockMap.empty()) return;
    
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

    std::string filename = std::to_string(hashChunkCoords(chunk.position.x / CHUNK_SIZE, chunk.position.y / CHUNK_SIZE, chunk.position.z / CHUNK_SIZE)) + ".chunk";
    std::cout << "Saving chunk to file: " << filename << std::endl;
    std::ofstream outputFile(CHUNK_FOLDER + "/" + filename, std::ios::out | std::ios::binary);

    if (!outputFile.is_open()) {
        std::cerr << "Error: Could not open file for writing." << std::endl;
        return;
    }

    size_t numBlocks = chunk.modifiedBlockMap.size();
    outputFile.write(reinterpret_cast<const char*>(&numBlocks), sizeof(numBlocks));

    for (const auto& pair : chunk.modifiedBlockMap) {
        // Write the uint64_t key and the uint8_t ID directly
        outputFile.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
        outputFile.write(reinterpret_cast<const char*>(&pair.second), sizeof(pair.second));
    }
    
    outputFile.close();
    std::cout << "Saved " << numBlocks << " modified blocks." << std::endl;
}

inline void readChunkFromFile(Chunk& chunk, int seed) {
    uint64_t hash = hashChunkCoords(chunk.position.x / CHUNK_SIZE, chunk.position.y / CHUNK_SIZE, chunk.position.z / CHUNK_SIZE);

    std::string CHUNK_FOLDER = "worlds/" + std::to_string(seed);
    std::string filename = std::to_string(hash) + ".chunk";
    std::string fullPath = CHUNK_FOLDER + "/" + filename;

    std::ifstream inputFile(fullPath, std::ios::in | std::ios::binary);

    if (!inputFile.is_open()) {
        return;
    }

    // 1. Read the number of blocks
    size_t numBlocks;
    inputFile.read(reinterpret_cast<char*>(&numBlocks), sizeof(numBlocks));

    // 2. Loop and read each block's key and ID
    for (size_t i = 0; i < numBlocks; ++i) {
        uint64_t key;
        uint8_t id;
        inputFile.read(reinterpret_cast<char*>(&key), sizeof(key));
        inputFile.read(reinterpret_cast<char*>(&id), sizeof(id));
        
        chunk.modifiedBlockMap[key] = id;
        
        int x, y, z;
        decodeChunkHash(key, x, y, z);

        chunk.blocks[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE] = id;
    }
    
    inputFile.close();
    std::cout << "Successfully read " << numBlocks << " modified blocks from " << fullPath << std::endl;
}