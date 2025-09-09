#include "physics_system.h"
#include <iostream>

MotionSystem::MotionSystem(World* world) : world(world) {}



void MotionSystem::update(std::unordered_map<unsigned int,TransformComponent> &transformComponents, std::unordered_map<unsigned int,PhysicsComponent> &physicsComponents, float dt) {
    
    for (auto& entity : physicsComponents) {
        TransformComponent& transform = transformComponents[entity.first];

        glm::vec3 newPos = transform.position + entity.second.velocity * dt;

        uint64_t hash = hashChunkCoords(newPos.x/CHUNK_SIZE, newPos.y/CHUNK_SIZE, newPos.z/CHUNK_SIZE);
        {
            std::shared_lock lock(world->loadedChunksMutex);
            if (!world->loadedChunks.count(hash)) continue;
        }

        transform.position = newPos;
        entity.second.velocity *= std::pow(1.0f - friction, dt);
    }
}