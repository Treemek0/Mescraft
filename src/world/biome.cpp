#include "biome.h"

std::unordered_map<BiomeType, Biome> biomes = {
    {BiomeType::Plains, {
        BiomeType::Plains, 40, 70,
        { {1.0, BlockType::Dirt} },
        0.4, 0.7, 0.3, 1.0
    }},
    {BiomeType::Desert, {
        BiomeType::Desert, 30, 50,
        { {1.0, BlockType::Sand} },
        0.7, 1.0, 0.0, 0.3
    }},
    {BiomeType::Mountains, {
        BiomeType::Mountains, 80, 150,
        { {1.0, BlockType::Cobblestone} },
        0.0, 0.4, 0.0, 0.4
    }},
    {BiomeType::Forest, {
        BiomeType::Forest, 40, 70,
        { {1.0, BlockType::Dirt} },
        0.4, 1.0, 0.5, 1.0
    }},
    {BiomeType::Ocean, {
        BiomeType::Ocean, 0, 20,
        { {1.0, BlockType::Water} },
        0.0, 0.6, 0.6, 1.0
    }},
    {BiomeType::Savana, {
        BiomeType::Savana, 80, 150,
        { {1.0, BlockType::Terracota} },
        0.7, 1.0, 0.5, 1.0
    }},
};


BiomeType getBiomeType(double temp, double moist) {
    for (auto& [type, biome] : biomes) {
        if (temp >= biome.minTemp && temp <= biome.maxTemp &&
            moist >= biome.minMoist && moist <= biome.maxMoist) {
            return type;
        }
    }

    // fallback if none match
    return BiomeType::Plains;
}

Biome getBiome(BiomeType type) {
    return biomes[type];
}