#include "world.h"
#include <iostream>

#include <random>
#include <unordered_set>

World::World(unsigned int seed) : seed(seed), noiseGenerator(seed){

}

void World::generateChunk(Chunk& chunk){
    uint64_t chunkSeed = uint32_t(
        int(chunk.position.x) * 73856093 ^
        int(chunk.position.y) * 19349663 ^
        int(chunk.position.z) * 83492791 ^
        seed * 141245
    );
    
    std::mt19937 rng(chunkSeed);

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    bool isChunkAir = true;

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            double noiseHeight = noiseGenerator.noise2D(chunk.position.x + x, chunk.position.z + z, 8, 0.01);

            double noiseTemp = noiseGenerator.noise2D(chunk.position.x + x, chunk.position.z + z, 4, 0.002, 1);
            double noiseMoist = noiseGenerator.noise2D(chunk.position.x + x, chunk.position.z + z, 4, 0.002, 2);
            
            int height = getHeight(noiseHeight, noiseTemp, noiseMoist);

            Biome biome = getBiome(getBiomeType(noiseTemp, noiseMoist));

            for (int y = 0; y < CHUNK_SIZE; ++y) {
                int blockY = chunk.position.y + y;

                if(blockY < height){
                    BlockType chosenBlock = BlockType::Air;

                    isChunkAir = false;

                    if(blockY < height - 3){
                        if(blockY < 0){
                            chosenBlock = BlockType::Dark_Stone;
                        }else{
                            chosenBlock = BlockType::Stone;
                        }
                    }else{
                        double r = dist(rng);
                        double sum = 0.0;

                        for (auto& [prob, block] : biome.blocksVariants) {
                            sum += prob;
                            if (r <= sum) {
                                chosenBlock = block;
                                break;
                            }
                        }

                        if(chosenBlock == BlockType::Dirt && chunk.position.y + y == height - 1) chosenBlock = BlockType::Grass_Block;
                    }

                    if(!noiseGenerator.caveAt(chunk.position.x + x, chunk.position.y + y, chunk.position.z + z, height)){
                        setBlockID(chunk, x, y, z, static_cast<int>(chosenBlock));
                    }
                }
            }
        }
    }

    if(!isChunkAir){
        std::unordered_map<glm::ivec3, BlockType, ivec3_hash> oresPositions;

        generateOres(oresPositions, chunk, chunkSeed);

        for (auto& [pos, block] : oresPositions) {
            int id = getBlockID(chunk, pos.x, pos.y, pos.z);
            if(id != static_cast<int>(BlockType::Dark_Stone) &&  id != static_cast<int>(BlockType::Stone)) continue;
            setBlockID(chunk, pos.x, pos.y, pos.z, static_cast<int>(block));
        }
    }

    readChunkFromFile(chunk, seed);
}

int World::getHeight(double noiseHeight, double noiseTemp, double noiseMoist) {
    double blendedHeight = 0.0;
    double totalWeight = 0.0;

    for (auto& [type, biome] : biomes) {
        double centerTemp  = (biome.minTemp + biome.maxTemp) / 2.0;
        double centerMoist = (biome.minMoist + biome.maxMoist) / 2.0;

        // distance in (temp, moist) space
        double dx = noiseTemp - centerTemp;
        double dy = noiseMoist - centerMoist;
        double dist = sqrt(dx*dx + dy*dy);

        // inverse distance weight (closer biomes contribute more)
        double weight = 1.0 / (dist + 0.0001); // avoid divide by zero

        // blend the biome height
        double biomeHeight = biome.minHeight + noiseHeight * (biome.maxHeight - biome.minHeight);
        blendedHeight += biomeHeight * weight;
        totalWeight += weight;
    }

    blendedHeight /= totalWeight; // normalize
    return static_cast<int>(blendedHeight);
}

void World::generateOres(std::unordered_map<glm::ivec3, BlockType, ivec3_hash>& oresPositions, Chunk& chunk, uint64_t chunkSeed){
    {
        uint64_t oreSeed = chunkSeed;
        std::mt19937 vrng(oreSeed);
        
        int coalVeins = std::uniform_int_distribution<>(1, 2)(vrng);

        for (int i = 1; i < coalVeins + 1; ++i) {
            std::mt19937 xrng(oreSeed * i + 1);
            std::mt19937 yrng(oreSeed * i + 2);
            std::mt19937 zrng(oreSeed * i + 3);
            int x = std::uniform_int_distribution<>(0, CHUNK_SIZE-1)(xrng);
            int y = std::uniform_int_distribution<>(0, CHUNK_SIZE-1)(yrng);
            int z = std::uniform_int_distribution<>(0, CHUNK_SIZE-1)(zrng);

            // number of ores in this vein
            int oreCount = std::uniform_int_distribution<>(5, 15)(vrng); // for example 3-8 ores

            float veinSize = std::uniform_real_distribution<>(2.0f, 4.0f)(vrng);

            int oresAdded = 0;
            bool isCountComplete = false;
            for (int dx = -veinSize; dx <= veinSize; dx++) {
                if(isCountComplete) break;

                for (int dy = -veinSize; dy <= veinSize; dy++) {
                    if(isCountComplete) break;

                    for (int dz = -veinSize; dz <= veinSize; dz++) {
                        float dist = sqrt(dx*dx + dy*dy + dz*dz);
                        if (dist <= veinSize) {
                            int px = x + dx;
                            int py = y + dy;
                            int pz = z + dz;

                            oresAdded++;
                            if (oresAdded > oreCount) {
                                isCountComplete = true;
                                break;
                            }

                            if (px >= 0 && px < CHUNK_SIZE && py >= 0 && py < CHUNK_SIZE && pz >= 0 && pz < CHUNK_SIZE) {
                                BlockType ore = (chunk.position.y >= 0) ? BlockType::Coal_Ore : BlockType::Dark_Coal_Ore;
                                oresPositions.emplace(glm::ivec3(px, py, pz), ore);
                            }
                        }
                    }
                }
            }
        }
    }

    {
        uint64_t oreSeed = chunkSeed + 1;
        std::mt19937 vrng(oreSeed);
        
        int veins = std::uniform_int_distribution<>(0, 2)(vrng);

        for (int i = 1; i < veins + 1; ++i) {
            std::mt19937 xrng(oreSeed * i + 1);
            std::mt19937 yrng(oreSeed * i + 2);
            std::mt19937 zrng(oreSeed * i + 3);
            int x = std::uniform_int_distribution<>(0, CHUNK_SIZE-1)(xrng);
            int y = std::uniform_int_distribution<>(0, CHUNK_SIZE-1)(yrng);
            int z = std::uniform_int_distribution<>(0, CHUNK_SIZE-1)(zrng);

            // number of ores in this vein
            int oreCount = std::uniform_int_distribution<>(3, 10)(vrng); // for example 3-8 ores

            float veinSize = std::uniform_real_distribution<>(1.0f, 4.0f)(vrng);

            int oresAdded = 0;
            bool isCountComplete = false;
            for (int dx = -veinSize; dx <= veinSize; dx++) {
                if(isCountComplete) break;

                for (int dy = -veinSize; dy <= veinSize; dy++) {
                    if(isCountComplete) break;

                    for (int dz = -veinSize; dz <= veinSize; dz++) {
                        float dist = sqrt(dx*dx + dy*dy + dz*dz);
                        if (dist <= veinSize) {
                            int px = x + dx;
                            int py = y + dy;
                            int pz = z + dz;

                            oresAdded++;
                            if (oresAdded > oreCount) {
                                isCountComplete = true;
                                break;
                            }

                            if (px >= 0 && px < CHUNK_SIZE && py >= 0 && py < CHUNK_SIZE && pz >= 0 && pz < CHUNK_SIZE) {
                                 BlockType ore = (chunk.position.y >= 0) ? BlockType::Iron_Ore : BlockType::Dark_Iron_Ore;
                                oresPositions.emplace(glm::ivec3(px, py, pz), ore);
                            }
                        }
                    }
                }
            }
        }
    }

    {
        uint64_t oreSeed = chunkSeed + 2;
        std::mt19937 vrng(oreSeed);
        
        int veins = std::uniform_int_distribution<>(0, 1)(vrng);

        for (int i = 1; i < veins + 1; ++i) {
            std::mt19937 xrng(oreSeed * i + 1);
            std::mt19937 yrng(oreSeed * i + 2);
            std::mt19937 zrng(oreSeed * i + 3);
            int x = std::uniform_int_distribution<>(0, CHUNK_SIZE-1)(xrng);
            int y = std::uniform_int_distribution<>(0, CHUNK_SIZE-1)(yrng);
            int z = std::uniform_int_distribution<>(0, CHUNK_SIZE-1)(zrng);

            // number of ores in this vein
            int oreCount = std::uniform_int_distribution<>(4, 7)(vrng); // for example 3-8 ores

            float veinSize = std::uniform_real_distribution<>(1.0f, 4.0f)(vrng);

            int oresAdded = 0;
            bool isCountComplete = false;
            for (int dx = -veinSize; dx <= veinSize; dx++) {
                if(isCountComplete) break;

                for (int dy = -veinSize; dy <= veinSize; dy++) {
                    if(isCountComplete) break;

                    for (int dz = -veinSize; dz <= veinSize; dz++) {
                        float dist = sqrt(dx*dx + dy*dy + dz*dz);
                        if (dist <= veinSize) {
                            int px = x + dx;
                            int py = y + dy;
                            int pz = z + dz;

                            oresAdded++;
                            if (oresAdded > oreCount) {
                                isCountComplete = true;
                                break;
                            }

                            if (px >= 0 && px < CHUNK_SIZE && py >= 0 && py < CHUNK_SIZE && pz >= 0 && pz < CHUNK_SIZE) {
                                BlockType ore = (chunk.position.y >= 0) ? BlockType::Gold_Ore : BlockType::Dark_Gold_Ore;
                                oresPositions.emplace(glm::ivec3(px, py, pz), ore);
                            }
                        }
                    }
                }
            }
        }
    }

    {
        if(chunk.position.y >= 16) return;

        uint64_t oreSeed = chunkSeed + 3;
        std::mt19937 vrng(oreSeed);
        
        int veins1 = std::uniform_int_distribution<>(0, 1)(vrng);
        int veins2 = std::uniform_int_distribution<>(0, 1)(vrng);
        int veins = veins1 * veins2;

        for (int i = 1; i < veins + 1; ++i) {
            std::mt19937 xrng(oreSeed * i + 1);
            std::mt19937 yrng(oreSeed * i + 2);
            std::mt19937 zrng(oreSeed * i + 3);
            int x = std::uniform_int_distribution<>(0, CHUNK_SIZE-1)(xrng);
            int y = std::uniform_int_distribution<>(0, CHUNK_SIZE-1)(yrng);
            int z = std::uniform_int_distribution<>(0, CHUNK_SIZE-1)(zrng);

            // number of ores in this vein
            int oreCount = std::uniform_int_distribution<>(3, 8)(vrng); // for example 3-8 ores

            float veinSize = std::uniform_real_distribution<>(2.0f, 4.0f)(vrng);

            int oresAdded = 0;
            bool isCountComplete = false;
            for (int dx = -veinSize; dx <= veinSize; dx++) {
                if(isCountComplete) break;

                for (int dy = -veinSize; dy <= veinSize; dy++) {
                    if(isCountComplete) break;

                    for (int dz = -veinSize; dz <= veinSize; dz++) {
                        float dist = sqrt(dx*dx + dy*dy + dz*dz);
                        if (dist <= veinSize) {
                            int px = x + dx;
                            int py = y + dy;
                            int pz = z + dz;

                            oresAdded++;
                            if (oresAdded > oreCount) {
                                isCountComplete = true;
                                break;
                            }

                            if (px >= 0 && px < CHUNK_SIZE && py >= 0 && py < CHUNK_SIZE && pz >= 0 && pz < CHUNK_SIZE) {
                                BlockType ore = (chunk.position.y >= 0) ? BlockType::Diamond_Ore : BlockType::Dark_Diamond_Ore;
                                oresPositions.emplace(glm::ivec3(px, py, pz), ore);
                            }
                        }
                    }
                }
            }
        }
    }
}