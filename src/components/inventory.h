#pragma once

struct Inventory {
    int selectedSlot = 0;
    int items[36] = {0};

    inline int getSelectedItem(){
        return items[selectedSlot];
    }
};

inline void addItem(Inventory* inv, int slot, int item){
    if(slot < 0 || slot >= 36) return;
    inv->items[slot] = item;
}


