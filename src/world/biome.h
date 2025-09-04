#pragma once
#include <unordered_map>
#include "./block.h"

enum class BiomeType {
    Plains,
    Desert,
    Mountains,
    Forest,
    Ocean,
    Savana
};

struct Biome {
    BiomeType type;
    int minHeight;
    int maxHeight;
    std::vector<std::pair<double, BlockType>> blocksVariants;
    double minTemp;
    double maxTemp;
    double minMoist;
    double maxMoist;
};

extern std::unordered_map<BiomeType, Biome> biomes;

BiomeType getBiomeType(double temp, double moist);
Biome getBiome(BiomeType type);
