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
    LogicSystem(GLFWwindow* window, World* w);

    ~LogicSystem();

    void update(float dt);
    void handlePlayerMouseClick(RaycastHit hit, std::mutex& chunkMapMutex, std::mutex& meshCreationQueueMutex, MeshSystem& meshSystem, std::unordered_map<uint64_t, Mesh>& chunksMesh);
    void updatePlayerSlotKeys(RenderSystem* renderSystem);
    void scroll(double xoffset, double yoffset);

    Player* player;
private:
    GLFWwindow* window;
    World* world;

    Keybind left_click;
    Keybind right_click;
    Keybind middle_click;
};