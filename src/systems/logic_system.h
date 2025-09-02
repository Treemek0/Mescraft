#pragma once
#include <player.h>
#include <raycast.h>
#include <world.h>
#include <mutex>
#include <mesh_system.h>
#include <GLFW/glfw3.h>
#include <unordered_map>


class RenderSystem;

class LogicSystem {
public:
    LogicSystem(GLFWwindow* window, World* w);

    ~LogicSystem();

    void update(float dt);
    void handlePlayerMouseClick(RaycastHit hit, std::mutex& chunkMapMutex, std::mutex& meshCreationQueueMutex, MeshSystem& meshSystem, std::unordered_map<uint64_t, Mesh>& chunksMesh);
    void updatePlayerSlotKeys(RenderSystem* renderSystem);

    Player* player;
private:
    GLFWwindow* window;
    World* world;
};