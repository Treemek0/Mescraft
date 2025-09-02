#include "physics_system.h"
#include <iostream>

void MotionSystem::update(std::unordered_map<unsigned int,TransformComponent> &transformComponents, std::unordered_map<unsigned int,PhysicsComponent> &physicsComponents, float dt) {
    
    for (auto& entity : physicsComponents) {
        transformComponents[entity.first].position += entity.second.velocity * dt;

        entity.second.velocity *= std::pow(1.0f - friction, dt);
    }
}