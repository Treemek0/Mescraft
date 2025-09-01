#pragma once
#include <glm/glm.hpp>

struct Block {
    glm::vec3 position;
    uint8_t id; // 1 - dirt
};

enum class BlockType : int8_t {
    Air = 0,
    Dirt = 1,
    Grass_Block = 2,
    Cobblestone = 3,
    Stone = 4,
    Sand = 5,
    Water = 6,
    Terracota = 7,
    Coal_Ore = 8,
    Iron_Ore = 9,
    Gold_Ore = 10,
    Diamond_Ore = 11
};