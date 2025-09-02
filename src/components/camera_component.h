#pragma once
#include <glm/glm.hpp>

struct CameraComponent {
    glm::vec3 right;
    glm::vec3 up;
    glm::vec3 forwards;

    glm::mat4 viewMatrix;
};