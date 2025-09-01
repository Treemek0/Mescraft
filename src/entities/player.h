#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Player {
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 velocity;
    float health;
    float hunger;

    Player() : health(20.0f), hunger(20.0f) {}
    
    void update(float deltaTime) {
        const float hungerDecayRate = 0.1f; // units per second

        hunger -= hungerDecayRate * deltaTime;

        hunger = glm::clamp(hunger, 0.0f, 100.0f);

        if (hunger <= 0.0f) {
            health -= 1.0f * deltaTime; // damage per second
            health = glm::max(health, 0.0f);
        }
    }
};