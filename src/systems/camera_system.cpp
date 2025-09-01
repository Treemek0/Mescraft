#include "camera_system.h"

CameraSystem::CameraSystem(unsigned int shader, GLFWwindow* window) {
    this->window = window;

    glUseProgram(shader);
    viewLocation = glGetUniformLocation(shader, "view");
}

void CameraSystem::update(
    std::unordered_map<unsigned int,TransformComponent> &transformComponents, std::unordered_map<unsigned int, PhysicsComponent> &physicsComponents, unsigned int cameraID, CameraComponent& cameraComponent, float dt) {

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glm::vec3& pos = transformComponents[cameraID].position;
    glm::vec3& vel = physicsComponents[cameraID].velocity;
    glm::vec3& eulers = transformComponents[cameraID].rotation;
    float pitch = glm::radians(eulers.x);
    float yaw = glm::radians(eulers.y);

    glm::vec3& right = cameraComponent.right;
    glm::vec3& up = cameraComponent.up;
    glm::vec3& forwards = cameraComponent.forwards;

    forwards = {sin(yaw) * cos(pitch), -sin(pitch), -cos(yaw) * cos(pitch)};

    right = glm::normalize(glm::cross(forwards, global_up));
    up = glm::normalize(glm::cross(right, forwards));

    glm::mat4 view = glm::lookAt(pos, pos + forwards, up);

    glUniformMatrix4fv(viewLocation, 1, GL_FALSE, glm::value_ptr(view));

    float speed = 5.0f;

    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        speed = 30.0f;
    }

    //Keys
    glm::vec3 dPos = {0.0f, 0.0f, 0.0f};
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        dPos.z += 1.0f;  // forward
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        dPos.z -= 1.0f;  // backward
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        dPos.x -= 1.0f;  // left
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        dPos.x += 1.0f;  // right
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        dPos.y += 1.0;  // jump up
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        dPos.y -= 1.0;  // go down
    }

    if (glm::length(dPos) > 0.1f) {
        dPos = glm::normalize(dPos);
        // Project forward movement to the XZ plane for FPS-style controls
        glm::vec3 flat_forwards = glm::normalize(glm::vec3(forwards.x, 0.0f, forwards.z));

        vel += speed * 5.0f * dt * dPos.z * flat_forwards ;
        vel += speed * 5.0f * dt * dPos.x * right;

        vel.y += speed * 5.0f * dt * dPos.y;
    }

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        if (!escapePressed) {
            escapePressed = true;
            cursorInGame = !cursorInGame;
            glfwSetInputMode(window, GLFW_CURSOR, cursorInGame ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            glfwSetCursorPos(window, width/2, height/2);
        }
    }else{
        escapePressed = false;
    }

    //Mouse
    if(cursorInGame) {
        glm::vec3 dEulers = {0.0f, 0.0f, 0.0f};
        double mouse_x, mouse_y;
        double x_sensitivity = 0.05;
        double y_sensitivity = 0.05;

        glfwGetCursorPos(window, &mouse_x, &mouse_y);
        glfwSetCursorPos(window, width/2, height/2);

        float dx = x_sensitivity * static_cast<float>(mouse_x - width / 2);
        float dy = y_sensitivity * static_cast<float>(mouse_y - height / 2);

        eulers.y += dx; // Yaw
        eulers.x += dy; // Pitch

        eulers.x = fminf(89.0f, fmaxf(-89.0f, eulers.x));

        if (eulers.y > 360) {
            eulers.y -= 360;
        }
        else if (eulers.y < 0) {
            eulers.y += 360;
        }
    }
}