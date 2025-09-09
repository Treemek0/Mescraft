#pragma once
#include <player.h>
#include <raycast.h>
#include <world.h>
#include <mutex>
#include <mesh_system.h>
#include <GLFW/glfw3.h>
#include <unordered_map>
#include <keybind.h>


class RenderSystem;

class LogicSystem {
public:
    LogicSystem(GLFWwindow* window, World* w, RenderSystem* renderSystem);

    ~LogicSystem();

    void update(float dt);
    void handlePlayerMouseClick(RaycastHit hit, std::shared_mutex& chunkMapMutex, std::shared_mutex& meshCreationQueueMutex, MeshSystem& meshSystem, std::unordered_map<uint64_t, Mesh>& chunksMesh);
    void updatePlayerSlotKeys();
    void scroll(double xoffset, double yoffset);

    Player* player;
    RenderSystem* renderSystem;
private:
    GLFWwindow* window;
    World* world;

    Keybind left_click;
    Keybind right_click;
    Keybind middle_click;
};