#pragma once
#include "../components/transform_component.h"
#include "../components/render_component.h"
#include <unordered_map>
#include <unordered_set>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../world/world.h"
#include <memory>
#include <string>
#include <GL/gl.h>
#include <mutex>
#include <thread>
#include <atomic> 
#include <future>
#include "camera_component.h"
#include "textureManager.h"

class LogicSystem;

class RenderSystem {
public:

    RenderSystem(unsigned int shaders[], GLFWwindow* window, World* w, std::unordered_map<unsigned int, TransformComponent> transformComponents, LogicSystem* logicSystem);
    
    ~RenderSystem();

    void update(std::unordered_map<unsigned int,TransformComponent> &transformComponents,std::unordered_map<unsigned int,RenderComponent> &renderComponents, CameraComponent& cameraComponent);
    void generate_world(const glm::vec3& playerPos);
    void generate_world_meshes();
    unsigned int make_texture(const char* filename);
    unsigned int make_texture_resized(const char* filename, float scale);
    void drawText(const std::string& text, float x, float y, float scale, const glm::vec4& color);
    void renderHoverBlock(glm::vec3 playerPos, glm::vec3 cameraDir, float eyeHeight);
    bool neighborsReady(uint64_t hash, const std::unordered_set<uint64_t>& loadedChunks);
    void drawCursor();
    void setUpBuffers();
    void saveWorld();
    void drawHandItem(std::unordered_map<unsigned int, TransformComponent>& transformComponents);
    void drawHotbar();
    void drawItem(float x, float y, float scale, int itemID);
    void drawHotbarText(std::string text);

    float getScaleForHeight(float desiredHeight);
    float getTextWidth(const std::string& text, float scale);
    float getTextHeight(float scale);

    void generate3DCubeMesh();

    TextureManager textureManager;
private:
    std::thread dataCreationThread;
    std::thread meshCreationThread;
    std::atomic<bool> runningCreationThread;    
    std::vector<uint64_t> chunksToGenerate;
    glm::vec3 cameraPosForThread; // Shared camera position for thread
    std::mutex cameraMutex;    
    std::mutex meshQueueMutex;
    std::shared_mutex meshCreationQueueMutex;
    std::mutex meshDeleteQueueMutex;

    std::mutex generatedChunksDeleteMutex;

    void processChunkGeneration();
    void processMeshQueue();
    GLuint textVAO = 0;
    GLuint textVBO = 0;
    GLuint textEBO = 0;

    GLuint cursorVAO = 0;
    GLuint cursorVBO = 0;

    GLuint slotVAO = 0;
    GLuint slotVBO = 0;

    unsigned int shaderText;
    unsigned int shader;
    unsigned int shader2D;
    unsigned int shader3D_hud;
    World* world;
    
    std::unordered_map<uint64_t, Mesh> chunksMesh;
    std::vector<std::pair<uint64_t, MeshData>> meshQueue;
    std::unordered_map<uint64_t, std::shared_ptr<Chunk>> meshCreationQueue;
    std::vector<uint64_t> chunksToDeleteQueue;

    MeshSystem meshSystem;
    unsigned int blocksTextureID;
    unsigned int hoverTextureID;
    unsigned int slotTextureID;
    unsigned int slotSelectedTextureID;
    GLFWwindow* window;
    LogicSystem* logicSystem;

    Mesh handItemMesh;

    int lastPlayerChunkX = INT_MAX;
    int lastPlayerChunkY = INT_MAX;
    int lastPlayerChunkZ = INT_MAX;

    double itemNameTextShowTime = 0;
    std::string itemNameText = "";
};