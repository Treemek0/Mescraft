#pragma once
#include <glm/glm.hpp>

struct PhysicsComponent {
    glm::vec3 velocity;
    glm::vec3 acceleration;

    // hitbox
    glm::vec3 aabbMin;
    glm::vec3 aabbMax;
};