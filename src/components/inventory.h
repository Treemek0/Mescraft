#pragma once
#include "item.h"

struct Inventory {
    int selectedSlot = 0;
    Item items[36];

    inline Item getSelectedItem(){
        return items[selectedSlot];
    }
};


