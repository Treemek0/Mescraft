#pragma once
#include <../systems/mesh_system.h>

struct RenderComponent {
    unsigned int material;
    Mesh mesh;
};