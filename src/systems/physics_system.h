#pragma once
#include "../components/transform_component.h"
#include "../components/physics_component.h"
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../world/world.h"

class MotionSystem {
public:
    void update(
        std::unordered_map<unsigned int,TransformComponent> &transformComponents,
        std::unordered_map<unsigned int,PhysicsComponent> &physicsComponents,
        float dt);

    bool checkCollision(const Chunk& chunk, const glm::vec3& pos, const glm::vec3& size);
    double friction = 0.98;
};