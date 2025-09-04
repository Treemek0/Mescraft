#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>
#include "../components/camera_component.h"
#include "../components/transform_component.h"
#include "../components/physics_component.h"

class CameraSystem {
public:

    CameraSystem(unsigned int shaders[], GLFWwindow* window);

    void update(std::unordered_map<unsigned int,TransformComponent> &transformComponents, std::unordered_map<unsigned int, PhysicsComponent> &physicsComponents, unsigned int cameraID, CameraComponent& cameraComponent, float dt);
    
private:
    unsigned int viewLocation;
    glm::vec3 global_up = {0.0f, 1.0f, 0.0f};
    bool cursorInGame = true;
    bool escapePressed = false;
    GLFWwindow* window;
};