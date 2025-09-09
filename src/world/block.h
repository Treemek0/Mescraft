#pragma once
#include <glm/glm.hpp>

struct BlockData {
    uint8_t rotation;
    uint8_t id;

    BlockData() : rotation(0), id(0) {}
};

struct Block {
    glm::vec3 position;

    BlockData data; // 1 - dirt
};

inline uint8_t getRotationFromNormal(const glm::ivec3& normal) {
    if(normal == glm::ivec3(0, 1, 0) || normal == glm::ivec3(0, -1, 0)) return 0; // clicked top or bottom → default rotation
    else if(normal == glm::ivec3(0, 0, -1)) return 1; 
    else if(normal == glm::ivec3(-1, 0, 0)) return 2;
    else if(normal == glm::ivec3(0, 0, 1))  return 3;
    else if(normal == glm::ivec3(1, 0, 0))  return 4;

    return 0;
}

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
    Diamond_Ore = 11,
    Dark_Stone = 12,
    Dark_Coal_Ore = 13,
    Dark_Iron_Ore = 14,
    Dark_Gold_Ore = 15,
    Dark_Diamond_Ore = 16,
    Oak = 17
};