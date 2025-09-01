#include "world.h"
#include <iostream>

#include <random>
#include <unordered_set>

World::World(unsigned int seed) : seed(seed), noiseGenerator(seed){

}

void World::generateChunk(Chunk& chunk){
    std::mt19937 rng(uint32_t(
        int(chunk.position.x) * 73856093 ^
        int(chunk.position.y) * 19349663 ^
        int(chunk.position.z) * 83492791
    ));

    std::uniform_real_distribution<double> dist(0.0, 1.0);

    int coalVeins = std::uniform_int_distribution<>(0, 1)(rng);

    std::unordered_set<glm::ivec3, ivec3_hash> coalPositions;

    if(coalVeins){
        int x = std::uniform_int_distribution<>(0, CHUNK_SIZE-1)(rng);
        int y = std::uniform_int_distribution<>(0, CHUNK_SIZE-1)(rng);
        int z = std::uniform_int_distribution<>(0, CHUNK_SIZE-1)(rng);

        // vein size
        float veinSize = std::uniform_real_distribution<>(2.0f, 4.0f)(rng);

        // create a small spherical blob of coal
        for (int dx = -veinSize; dx <= veinSize; dx++) {
            for (int dy = -veinSize; dy <= veinSize; dy++) {
                for (int dz = -veinSize; dz <= veinSize; dz++) {
                    float dist = sqrt(dx*dx + dy*dy + dz*dz);
                    if (dist <= veinSize) {
                        int px = x + dx;
                        int py = y + dy;
                        int pz = z + dz;

                        coalPositions.insert(glm::ivec3(px, py, pz));
                    }
                }
            }
        }
    }

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            double noiseHeight = noiseGenerator.noise2D(chunk.position.x + x, chunk.position.z + z, 8, 0.01);

            double noiseTemp = noiseGenerator.noise2D(chunk.position.x + x, chunk.position.z + z, 4, 0.002, 1);
            double noiseMoist = noiseGenerator.noise2D(chunk.position.x + x, chunk.position.z + z, 4, 0.002, 2);
            
            int height = getHeight(noiseHeight, noiseTemp, noiseMoist);

            Biome biome = getBiome(getBiomeType(noiseTemp, noiseMoist));

            for (int y = 0; y < CHUNK_SIZE; ++y) {
                if(chunk.position.y + y < height){
                    double r = dist(rng);
                    double sum = 0.0;
                    BlockType chosenBlock = BlockType::Air;

                    if(chunk.position.y + y < height - 3){
                        if(coalPositions.count(glm::ivec3{x, y, z})){
                            chosenBlock = BlockType::Coal_Ore;
                        }else{
                            chosenBlock = BlockType::Stone;
                        }
                    }else{
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
