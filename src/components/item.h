#pragma once
#include <string>
#include <unordered_map>

enum class ItemType {
    Block, Decorative, Item, null
};

struct ItemInfo {
    int maxStack;
    std::string defaultName;
    ItemType type;
};

inline const std::unordered_map<int, ItemInfo> ItemCatalog = {
    { 0, { 0, "", ItemType::Block } },
    { 1, { 64, "Dirt", ItemType::Block } },
    { 2, { 64, "Grass", ItemType::Block } },
    { 3, { 64, "Cobblestone", ItemType::Block } },
    { 4, { 64, "Stone", ItemType::Block } },
    { 5, { 64, "Sand", ItemType::Block } },
    { 6, { 0, "Water", ItemType::Block } },
    { 7, { 64, "Terracota", ItemType::Block } },
    { 8, { 64, "Coal Ore", ItemType::Block } },
    { 9, { 64, "Iron Ore", ItemType::Block } },
    { 10, { 64, "Gold Ore", ItemType::Block } },
    { 11, { 64, "Diamond Ore", ItemType::Block } },
    { 12, { 64, "Dark Stone", ItemType::Block } },
    { 13, { 64, "Dark Coal Ore", ItemType::Block } },
    { 14, { 64, "Dark Iron Ore", ItemType::Block } },
    { 15, { 64, "Dark Gold Ore", ItemType::Block } },
    { 16, { 64, "Dark Diamond Ore", ItemType::Block } },
    { 17, { 64, "Oak", ItemType::Block } },
};

struct Item {
    int id;
    ItemType type;
    std::string name;
    int maxStack;
    int currentStack;

    Item(int id, int stack, const std::string& customName = "")
        : id(id),
          type(ItemCatalog.at(id).type),
          name(customName.empty() ? ItemCatalog.at(id).defaultName : customName),
          maxStack(ItemCatalog.at(id).maxStack),
          currentStack(stack) {}

    Item() : id(0), type(ItemType::null),
         name(""),
         maxStack(0),
         currentStack(0) {}
};
