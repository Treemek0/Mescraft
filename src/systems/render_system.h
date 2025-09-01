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
#include <string>
#include <GL/gl.h>
#include <mutex>
#include <thread>
#include <atomic> 
#include <future>

class RenderSystem {
public:

    RenderSystem(unsigned int shader, unsigned int shaderText, unsigned int shader2D, GLFWwindow* window, World* w, std::unordered_map<unsigned int, TransformComponent> transformComponents);
    
    ~RenderSystem();

    void update(std::unordered_map<unsigned int,TransformComponent> &transformComponents,std::unordered_map<unsigned int,RenderComponent> &renderComponents);
    void generate_world(const glm::vec3& playerPos);
    void generate_world_meshes();
    unsigned int make_texture(const char* filename);
    unsigned int make_texture_resized(const char* filename, float scale);
    void drawText(const std::string& text, float x, float y, float scale, const glm::vec3& color);
    void renderHoverBlock(glm::vec3 playerPos, glm::vec3 cameraDir, float eyeHeight);
    bool neighborsReady(uint64_t hash, const std::unordered_map<uint64_t, Chunk>& chunkMap);
    void drawCursor();
    void setUpBuffers();
    void saveWorld();

    std::vector<unsigned int> textures;

private:
    std::thread dataCreationThread;
    std::thread meshCreationThread;
    std::atomic<bool> runningCreationThread;    
    glm::vec3 cameraPosForThread; // Shared camera position for thread
    std::mutex cameraMutex;    

    std::mutex meshQueueMutex;
    std::mutex chunkMapMutex;
    std::mutex meshCreationQueueMutex;
    std::mutex meshDeleteQueueMutex;

    std::mutex generatedChunksDeleteMutex;

    void processMeshQueue();
    GLuint textVAO = 0;
    GLuint textVBO = 0;
    GLuint textEBO = 0;

    GLuint cursorVAO = 0;
    GLuint cursorVBO = 0;

    unsigned int shaderText;
    unsigned int shader;
    unsigned int shader2D;
    World* world;
    
    std::unordered_map<uint64_t, Mesh> chunksMesh;
    std::vector<std::pair<uint64_t, MeshData>> meshQueue;
    std::unordered_map<uint64_t, Chunk> meshCreationQueue;
    std::unordered_set<uint64_t> dirtyChunksQueue;
    std::vector<uint64_t> chunksToDeleteQueue;

    MeshSystem meshSystem;
    unsigned int blocksTextureID;
    unsigned int hoverTextureID;
    unsigned int modelLocation;
    GLFWwindow* window;

    int lastPlayerChunkX = INT_MAX;
    int lastPlayerChunkY = INT_MAX;
    int lastPlayerChunkZ = INT_MAX;
};