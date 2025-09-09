#pragma once
#include "PerlinNoise.hpp"
#include "SimplexNoise.h"
#include <unordered_map>
#include <unordered_set>
#include "./chunk.h"
#include <memory>
#include "./biome.h"
#include <shared_mutex>

class NoiseGenerator {
public:
    NoiseGenerator(unsigned int seed) : perlin(seed) {}

    double noise2D(double x, double y, int octaves, double scale) {
        double height = perlin.octave2D_01(x * scale, y * scale, octaves);
        return height*height;
    }
    
    double noise2D(double x, double y, int octaves, double scale, int add_to_seed) {
        double height = perlin.octave2D_01(x * scale, y * scale, octaves, 0.2);
        return height*height;
    }

    double noise3D(double x, double y, double z, double scale, int octaves, double persistence) {
        double value = 0.0;
        double amplitude = 1.0;
        double frequency = scale;
        double maxAmplitude = 0.0;

        // Fractal 3D noise
        for(int i = 0; i < octaves; ++i) {
            value += noise.noise(x * frequency, y * frequency, z * frequency) * amplitude;
            maxAmplitude += amplitude;
            amplitude *= persistence;
            frequency *= 1.5;
        }

        value /= maxAmplitude;        // [-1,1]
        value = (value + 1.0) * 0.5;  // [0,1]

        return value;
    }

    bool caveAt(int x, int y, int z, int worldHeight) {
        double interval = 0.85;

        double minHeight = worldHeight - 8;
        
        double surfaceFactor = 1.0 - (y - minHeight) / (worldHeight - minHeight);
        surfaceFactor = std::clamp(surfaceFactor, 0.0, 1.0);

        interval -= 0.25 * surfaceFactor;

        double bigCavesY = -30;
        double smallCavesY = std::min(worldHeight - 20, 30);
        double depthFactor = 1.0 - (y - bigCavesY) / (smallCavesY - bigCavesY);
        depthFactor = std::clamp(depthFactor, 0.0, 1.0);

        double cave = noise3D(x, y, z, 0.07 - (0.03 * depthFactor), 4, 0.3);

        return (cave > interval);
    }

private:
    siv::PerlinNoise perlin;
    SimplexNoise noise; 
};

struct ivec3_hash {
    std::size_t operator()(const glm::ivec3& v) const noexcept {
        // Combine x, y, z into a single hash using bit-mixing
        std::size_t h1 = std::hash<int>()(v.x);
        std::size_t h2 = std::hash<int>()(v.y);
        std::size_t h3 = std::hash<int>()(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class World {
public:
    int seed;
    std::unordered_map<uint64_t, std::shared_ptr<Chunk>> chunkMap;
    std::shared_mutex chunkMapMutex;

    std::unordered_set<uint64_t> loadedChunks;
    std::shared_mutex loadedChunksMutex;

    NoiseGenerator noiseGenerator;

    World(unsigned int seed);

    void generateChunk(Chunk& chunk);
    int getHeight(double noiseHeight, double noiseTemp, double noiseMoist);

private:
    void generateOres(std::unordered_map<glm::ivec3, BlockType, ivec3_hash>& oresPositions, Chunk& chunk, uint64_t chunkSeed);
};