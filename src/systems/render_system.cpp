#include "render_system.h"
#include <vector>
#include <iostream>
#include <GL/gl.h>
#include "../controller/app.h"
#include "../world/world.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>
#include <thread>
#include <../utilities/raycast.h>

#include <glm/gtc/matrix_transform.hpp>
#include "logic_system.h"
#include <frustum.h>

stbtt_bakedchar cdata[96];
GLuint fontTexture;
std::vector<unsigned char> fontBuffer;
int fontHeight = 32;

RenderSystem::RenderSystem(unsigned int shaders[], GLFWwindow* window, World* w, std::unordered_map<unsigned int, TransformComponent> transformComponents, LogicSystem* logicSystem) : world(w), runningCreationThread(true) {
    this->shader = shaders[0];
    this->shader2D = shaders[1];
    this->shaderText = shaders[2];
    this->shader3D_hud = shaders[3];

    blocksTextureID = textureManager.loadTexture("textures/blocks.png", TextureWrap::CLAMP_TO_EDGE, TextureWrap::CLAMP_TO_EDGE, TextureFilter::NEAREST, TextureFilter::NEAREST);
    hoverTextureID = textureManager.loadTexture("textures/hover.png", TextureWrap::CLAMP_TO_EDGE, TextureWrap::CLAMP_TO_EDGE, TextureFilter::NEAREST, TextureFilter::NEAREST);
    slotTextureID = textureManager.loadTexture("textures/slot.png", TextureWrap::CLAMP_TO_EDGE, TextureWrap::CLAMP_TO_EDGE, TextureFilter::NEAREST, TextureFilter::NEAREST);
    slotSelectedTextureID = textureManager.loadTexture("textures/slot_selected.png", TextureWrap::CLAMP_TO_EDGE, TextureWrap::CLAMP_TO_EDGE, TextureFilter::NEAREST, TextureFilter::NEAREST);
    this->window = window;
    this->logicSystem = logicSystem;

    setUpBuffers();

    {
        std::lock_guard<std::mutex> lock(cameraMutex);
        if(transformComponents.count(App::cameraID)){
            cameraPosForThread = transformComponents[App::cameraID].position;
        }
    }

    dataCreationThread = std::thread([this]() {
        while (runningCreationThread) {
            glm::vec3 cameraPos;
            {
                std::lock_guard<std::mutex> lock(cameraMutex);
                cameraPos = this->cameraPosForThread; // updated in update()
            }
            generate_world(cameraPos);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    meshCreationThread = std::thread([this]() {
        while (runningCreationThread) {
            generate_world_meshes();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}
    
bool wireframe = false;
bool wireframeClicked = true;
void RenderSystem::update(std::unordered_map<unsigned int, TransformComponent> &transformComponents, std::unordered_map<unsigned int, RenderComponent> &renderComponents, CameraComponent& cameraComponent) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    if(glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS){
        if(!wireframeClicked){
            wireframeClicked = true;
            wireframe = !wireframe;
        }
    }else{
        wireframeClicked = false;
    }
    
    logicSystem->updatePlayerSlotKeys(this);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    {
        std::lock_guard<std::mutex> lock(cameraMutex);
        if(transformComponents.count(App::cameraID)){
            cameraPosForThread = transformComponents[App::cameraID].position;
        }
    }

    processMeshQueue(); 

    glm::mat4 model = glm::mat4(1.0f);
    unsigned int modelLocation = glGetUniformLocation(shader, "model");
    glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));

    if(wireframe){
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    float aspect = static_cast<float>(width) / static_cast<float>(height);
	glm::mat4 projection = glm::perspective(App::fov, aspect, 0.01f, 1000.0f);

    Frustum frustum = extractFrustum(projection * cameraComponent.viewMatrix);
    for (auto& pair : chunksMesh) {
        Mesh& mesh = pair.second;

        if(isBoxInFrustum(frustum, mesh.startPositonOfChunk, mesh.startPositonOfChunk + glm::vec3(CHUNK_SIZE))){
            glBindVertexArray(mesh.VAO);
            glBindTexture(GL_TEXTURE_2D, blocksTextureID);
            glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
        }
    }
    
    glPolygonMode(GL_FRONT, GL_FILL);

    // rendering entites
    for (auto& entity : renderComponents) {
        TransformComponent& transform = transformComponents[entity.first];
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, transform.position);
        model = glm::rotate(model, glm::radians(transform.rotation.x), {1.0f, 0.0f, 0.0f});
        model = glm::rotate(model, glm::radians(transform.rotation.y), {0.0f, 1.0f, 0.0f});
        model = glm::rotate(model, glm::radians(transform.rotation.z), {0.0f, 0.0f, 1.0f});

        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
        
        glBindTexture(GL_TEXTURE_2D, entity.second.material);
        glBindVertexArray(entity.second.mesh.VAO);
        glDrawElements(GL_TRIANGLES, entity.second.mesh.indexCount, GL_UNSIGNED_INT, 0);
    }

    float yaw   = glm::radians(transformComponents[App::cameraID].rotation.y);
    float pitch = glm::radians(transformComponents[App::cameraID].rotation.x);

    glm::vec3 cameraDir;
    cameraDir.x = cos(pitch) * sin(yaw);
    cameraDir.y = -sin(pitch);
    cameraDir.z = -cos(pitch) * cos(yaw);
    cameraDir = glm::normalize(cameraDir);

    renderHoverBlock(transformComponents[App::cameraID].position, cameraDir, 0);

    std::string coordinates = "X: " + std::to_string((int)transformComponents[App::cameraID].position.x) + 
                              " Y: " + std::to_string((int)transformComponents[App::cameraID].position.y) + 
                              " Z: " + std::to_string((int)transformComponents[App::cameraID].position.z);
    drawText(coordinates, 5.0f, 0.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));
    drawText(std::to_string(App::fps), 5.0f, fontHeight, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));
    
    drawHandItem(transformComponents);
    drawHotbar();
    drawCursor();

	glfwSwapBuffers(window);
}

void RenderSystem::processMeshQueue() {
    // Process new meshes to be created (OpenGL calls on main thread)
    std::lock_guard<std::mutex> meshLock(meshQueueMutex);
    for (auto& [hash, meshData] : meshQueue) {
        Mesh mesh = meshSystem.createMesh(meshData); // OpenGL call on main thread

        int x, y, z;
        decodeChunkHash(hash, x, y, z);
        x *= CHUNK_SIZE;
        y *= CHUNK_SIZE;
        z *= CHUNK_SIZE;

        mesh.startPositonOfChunk = {x, y, z};

        chunksMesh[hash] = std::move(mesh);
    }
    meshQueue.clear();

    std::lock_guard<std::mutex> deletionLock(meshDeleteQueueMutex);
    for (uint64_t hash : chunksToDeleteQueue) {
        auto it = chunksMesh.find(hash);
        if (it != chunksMesh.end()) {
            meshSystem.deleteMesh(it->second);
            chunksMesh.erase(it);
        }
    }
    chunksToDeleteQueue.clear();
}

void RenderSystem::generate_world(const glm::vec3& playerPos) {
    auto& chunkMap = world->chunkMap;

    int playerChunkX = static_cast<int>(floor(playerPos.x / CHUNK_SIZE));
    int playerChunkY = static_cast<int>(floor(playerPos.y / CHUNK_SIZE));
    int playerChunkZ = static_cast<int>(floor(playerPos.z / CHUNK_SIZE));

    // Only update chunks if the player has moved to a new chunk
    if (playerChunkX == lastPlayerChunkX && playerChunkY == lastPlayerChunkY && playerChunkZ == lastPlayerChunkZ) {
        return;
    }

    lastPlayerChunkX = playerChunkX;
    lastPlayerChunkY = playerChunkY;
    lastPlayerChunkZ = playerChunkZ;

    int render_dist_h = RENDER_DISTANCE / 2;
    int render_dist_v = VERTICAL_RENDER_DISTANCE / 2;

    // Load a 1-chunk border around the render distance for seamless meshing
    const int load_dist_h = render_dist_h + 1;
    const int load_dist_v = render_dist_v + 1;

    // --- 1. Unload distant chunks ---
    {
        std::lock_guard<std::mutex> lock(chunkMapMutex);
        for (auto it = chunkMap.begin(); it != chunkMap.end(); ) {
            int cx, cy, cz;
            decodeChunkHash(it->first, cx, cy, cz);

            if (abs(cx - playerChunkX) > load_dist_h || abs(cy - playerChunkY) > load_dist_v || abs(cz - playerChunkZ) > load_dist_h) {
                {
                    std::lock_guard<std::mutex> lock(meshDeleteQueueMutex);
                    chunksToDeleteQueue.push_back(it->first);
                }

                writeChunkToFile(it->second, world->seed);
                it = chunkMap.erase(it);
            } else {
                ++it;
            }
        }
    }

    // --- 2. Load new chunk data ---
    for (int cz = playerChunkZ - load_dist_h; cz <= playerChunkZ + load_dist_h; ++cz) {
        for (int cx = playerChunkX - load_dist_h; cx <= playerChunkX + load_dist_h; ++cx) {
            for (int cy = playerChunkY + load_dist_v - 1; cy >= playerChunkY - load_dist_v; --cy) {
                uint64_t hash = hashChunkCoords(cx, cy, cz);
                
                
                bool chunk_exists;
                {
                    std::lock_guard<std::mutex> lock(chunkMapMutex);
                    chunk_exists = (chunkMap.find(hash) != chunkMap.end());
                }

                if (!chunk_exists) {
                    Chunk chunk;
                    chunk.position = { static_cast<float>(cx * CHUNK_SIZE),
                                       static_cast<float>(cy * CHUNK_SIZE),
                                       static_cast<float>(cz * CHUNK_SIZE) };

                    // Heavy chunk creating
                    world->generateChunk(chunk);

                    {
                        std::lock_guard<std::mutex> lock(chunkMapMutex);
                        chunkMap.emplace(hash, std::move(chunk));
                    }

                    if(cz >= playerChunkZ - render_dist_h && cz <= playerChunkZ + render_dist_h && cy >= playerChunkY - render_dist_v && cy <= playerChunkY + render_dist_v && cx >= playerChunkX - render_dist_h && cx <= playerChunkX + render_dist_h){
                        // Lock both mutexes at once to prevent deadlock
                        std::scoped_lock lock(chunkMapMutex, meshCreationQueueMutex);
                        meshCreationQueue.emplace(hash, chunkMap.at(hash));
                    }else{
                        dirtyChunksQueue.insert(hash);
                    }
                }else{
                    if(cz >= playerChunkZ - render_dist_h && cz <= playerChunkZ + render_dist_h && cy >= playerChunkY - render_dist_v && cy <= playerChunkY + render_dist_v && cx >= playerChunkX - render_dist_h && cx <= playerChunkX + render_dist_h){
                        if(dirtyChunksQueue.count(hash) == 1){
                           // Lock both mutexes at once to prevent deadlock
                           std::scoped_lock lock(chunkMapMutex, meshCreationQueueMutex);
                           meshCreationQueue.emplace(hash, chunkMap.at(hash));
                           dirtyChunksQueue.erase(hash);
                        }
                    }
                }
            }
        }
    }
}


void RenderSystem::generate_world_meshes() {
    std::lock_guard<std::mutex> lock(meshCreationQueueMutex);
    for (auto it = meshCreationQueue.begin(); it != meshCreationQueue.end(); ) {
        uint64_t hash = it->first;

        {
            std::lock_guard<std::mutex> chunkLock(chunkMapMutex);
            if (neighborsReady(hash, world->chunkMap)) {
                double lastTime = glfwGetTime();
                MeshData meshData = meshSystem.createChunkData(meshCreationQueue.at(hash), world->chunkMap);
                std::lock_guard<std::mutex> meshLock(meshQueueMutex);
                meshQueue.push_back({hash, meshData});

                it = meshCreationQueue.erase(it);
            }else{
                it++;
            }
        }
    }
}

const int neighborOffsets[6][3] = {
    {1, 0, 0}, {-1, 0, 0},
    {0, 1, 0}, {0, -1, 0},
    {0, 0, 1}, {0, 0, -1}
};

bool RenderSystem::neighborsReady(uint64_t hash, const std::unordered_map<uint64_t, Chunk>& chunkMap) {
    int cx, cy, cz;
    decodeChunkHash(hash, cx, cy, cz);

    for (auto& offset : neighborOffsets) {
        uint64_t nHash = hashChunkCoords(cx + offset[0], cy + offset[1], cz + offset[2]);
        if (!chunkMap.count(nHash)) return false;
    }
    return true;
}


void RenderSystem::drawText(const std::string& text, float x, float y, float scale, const glm::vec3& color) 
{
     // --- State setup for 2D rendering ---
    glDisable(GL_CULL_FACE); // Disable culling for 2D text quads
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST); // UI should always be drawn on top

    glUseProgram(shaderText);

    // Set text color
    glUniform3f(glGetUniformLocation(shaderText, "textColor"), color.r, color.g, color.b);

    // Bind font texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glUniform1i(glGetUniformLocation(shaderText, "textAtlas"), 0);

    glBindVertexArray(textVAO);

    // --- Generate vertex buffer for the text ---
    std::vector<float> vertices;
    vertices.reserve(text.length() * 6 * 4);

    y += fontHeight * scale; // Adjust starting y position based on font size and scale

    for (const char* p = text.c_str(); *p; p++)
    {
        if (*p >= 32 && *p < 128)
        {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(cdata, 512, 512, *p - 32, &x, &y, &q, 1);

            float x0 = q.x0 * scale;
            float y0 = q.y0 * scale;
            float x1 = q.x1 * scale;
            float y1 = q.y1 * scale;

            vertices.insert(vertices.end(), {
                // Triangle 1
                x0, y0,   q.s0, q.t0, // Top-Left
                x1, y1,   q.s1, q.t1, // Bottom-Right
                x0, y1,   q.s0, q.t1, // Bottom-Left
                // Triangle 2
                x0, y0,   q.s0, q.t0, // Top-Left
                x1, y0,   q.s1, q.t0, // Top-Right
                x1, y1,   q.s1, q.t1  // Bottom-Right
            });
        }
    }

    // --- Draw the text ---
    if (!vertices.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(vertices.size() / 4));
    }

    // --- Restore state ---
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); 
    glDisable(GL_BLEND);

    glUseProgram(shader);
}

void RenderSystem::renderHoverBlock(glm::vec3 playerPos, glm::vec3 cameraDir, float eyeHeight){
    RaycastHit hit = raycast(playerPos + glm::vec3(0, eyeHeight, 0), cameraDir, 5.0f, world->chunkMap);

    if (hit.hit) {
        int x = hit.block.position.x;
        int y = hit.block.position.y;
        int z = hit.block.position.z;

        float offset = 0.001f;
        uint8_t layer = 255; // 4 - full light on block

        Vertex vertices[] = {
            // Front (+Z)
            {{x-offset, y-offset, z+1+offset}, {0.0f, 0.0f}, layer},
            {{x+1+offset, y-offset, z+1+offset}, {1.0f, 0.0f}, layer},
            {{x+1+offset, y+1+offset, z+1+offset}, {1.0f, 1.0f}, layer},
            {{x-offset, y+1+offset, z+1+offset}, {0.0f, 1.0f}, layer},

            // Back (-Z)
            {{x+1+offset, y-offset, z-offset}, {0.0f, 0.0f}, layer},
            {{x-offset, y-offset, z-offset}, {1.0f, 0.0f}, layer},
            {{x-offset, y+1+offset, z-offset}, {1.0f, 1.0f}, layer},
            {{x+1+offset, y+1+offset, z-offset}, {0.0f, 1.0f}, layer},

            // Left (-X)
            {{x-offset, y-offset, z-offset}, {0.0f, 0.0f}, layer},
            {{x-offset, y-offset, z+1+offset}, {1.0f, 0.0f}, layer},
            {{x-offset, y+1+offset, z+1+offset}, {1.0f, 1.0f}, layer},
            {{x-offset, y+1+offset, z-offset}, {0.0f, 1.0f}, layer},

            // Right (+X)
            {{x+1+offset, y-offset, z+1+offset}, {0.0f, 0.0f}, layer},
            {{x+1+offset, y-offset, z-offset}, {1.0f, 0.0f}, layer},
            {{x+1+offset, y+1+offset, z-offset}, {1.0f, 1.0f}, layer},
            {{x+1+offset, y+1+offset, z+1+offset}, {0.0f, 1.0f}, layer},

            // Top (+Y)
            {{x-offset, y+1+offset, z-offset}, {0.0f, 0.0f}, layer},
            {{x+1+offset, y+1+offset, z-offset}, {1.0f, 0.0f}, layer},
            {{x+1+offset, y+1+offset, z+1+offset}, {1.0f, 1.0f}, layer},
            {{x-offset, y+1+offset, z+1+offset}, {0.0f, 1.0f}, layer},

            // Bottom (-Y)
            {{x-offset, y-offset, z+1+offset}, {0.0f, 0.0f}, layer},
            {{x+1+offset, y-offset, z+1+offset}, {1.0f, 0.0f}, layer},
            {{x+1+offset, y-offset, z-offset}, {1.0f, 1.0f}, layer},
            {{x-offset, y-offset, z-offset}, {0.0f, 1.0f}, layer}
        };


        unsigned int indices[] = {
            // Front (+Z)
            0, 1, 2,
            2, 3, 0,

            // Back (-Z)
            4, 5, 6,
            6, 7, 4,

            // Left (-X)
            8, 9, 10,
            10, 11, 8,

            // Right (+X)
            12, 13, 14,
            14, 15, 12,

            // Top (+Y)
            16, 18, 17,
            18, 16, 19,

            // Bottom (-Y)
            20, 22, 21,
            22, 20, 23
        };

        MeshData data;
        data.vertices.assign(vertices, vertices + sizeof(vertices)/sizeof(Vertex));
        data.indices.assign(indices, indices + sizeof(indices)/sizeof(unsigned int));

        Mesh hoverMesh = meshSystem.createMesh(data);
        glEnable(GL_BLEND);
        glBindVertexArray(hoverMesh.VAO);
        glBindTexture(GL_TEXTURE_2D, hoverTextureID);
        glDrawElements(GL_TRIANGLES, hoverMesh.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glDisable(GL_BLEND);
        meshSystem.deleteMesh(hoverMesh);

        // breaking and placing blocks
        logicSystem->handlePlayerMouseClick(hit, chunkMapMutex, meshCreationQueueMutex, meshSystem, chunksMesh);
    }
}

void RenderSystem::drawCursor() {
    glDisable(GL_CULL_FACE); // Disable culling for 2D text quads
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST); // UI should always be drawn on top

    glUseProgram(shader2D);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    glUniform1i(glGetUniformLocation(shader2D, "useTexture"), 0);  // plain color
    glUniform4f(glGetUniformLocation(shader2D, "color"), 1.0f, 1.0f, 1.0f, 1.0f); // white

    float cursorSize = 1.5f;
    glUniform2f(glGetUniformLocation(shader2D, "scale"), cursorSize, cursorSize);
    glUniform2f(glGetUniformLocation(shader2D, "offset"), width / 2.0f, height / 2.0f);

    glBindVertexArray(cursorVAO);
    glDrawArrays(GL_LINES, 0, 4);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); 
    glDisable(GL_BLEND);

    glUseProgram(shader);
}

void RenderSystem::drawHandItem(std::unordered_map<unsigned int, TransformComponent>& transformComponents) {
    if(logicSystem->player->inventory.getSelectedItem() == 0) return;

    glClear(GL_DEPTH_BUFFER_BIT);

    TransformComponent& cameraTransform = transformComponents.at(App::cameraID);
    glm::vec3 cameraPos = cameraTransform.position;

    glm::vec3 cameraDir;
    float yaw   = glm::radians(cameraTransform.rotation.y);
    float pitch = glm::radians(cameraTransform.rotation.x);
    cameraDir.x = cos(pitch) * sin(yaw);
    cameraDir.y = -sin(pitch);
    cameraDir.z = -cos(pitch) * cos(yaw);
    cameraDir = glm::normalize(cameraDir);

    glm::mat4 mainView = glm::lookAt(cameraPos, cameraPos + cameraDir, glm::vec3(0.0f, 1.0f, 0.0f));

    glUseProgram(shader3D_hud);

    glm::mat4 handItemView = glm::mat4(glm::mat3(mainView));
    glUniformMatrix4fv(glGetUniformLocation(shader3D_hud, "view"), 1, GL_FALSE, glm::value_ptr(handItemView));

    int itemID = logicSystem->player->inventory.getSelectedItem();
    float uSize = meshSystem.blockTexSize / static_cast<float>(meshSystem.atlasWidthPixels);
    float vSize = meshSystem.blockTexSize / static_cast<float>(meshSystem.atlasHeightPixels);
    glUniform2f(glGetUniformLocation(shader3D_hud, "tileSize"), uSize, vSize);

    float yOffset = (itemID - 1) * vSize;
    glUniform2f(glGetUniformLocation(shader3D_hud, "tileOffset"), 0.0f, yOffset);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, glm::radians(-cameraTransform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(-cameraTransform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));

    model = glm::translate(model, glm::vec3(0.7f, -0.6f, -0.5f)); 
    model = glm::rotate(model, glm::radians(-10.0f), glm::vec3(1.0f, 0.0f, 0.0f)); 
    model = glm::rotate(model, glm::radians(-10.0f), glm::vec3(0.0f, 1.0f, 0.0f)); 
    model = glm::scale(model, glm::vec3(0.3f)); 
    glUniformMatrix4fv(glGetUniformLocation(shader3D_hud, "model"), 1, GL_FALSE, glm::value_ptr(model));

    glBindTexture(GL_TEXTURE_2D, blocksTextureID);
    glBindVertexArray(handItemMesh.VAO);
    glDrawElements(GL_TRIANGLES, handItemMesh.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // 4. Switch back to the main world shader for subsequent rendering.
    glUseProgram(shader);
}

RenderSystem::~RenderSystem() {
    runningCreationThread = false; // Signal the worker thread to stop
    if (dataCreationThread.joinable()) {
        dataCreationThread.join(); // Wait for the worker thread to finish
    }
    if (meshCreationThread.joinable()) {
        meshCreationThread.join();
    }

    // Clean up all chunk meshes
    for (auto& pair : chunksMesh) {
        meshSystem.deleteMesh(pair.second);
    }
    chunksMesh.clear();

    // Clean up other meshes
    if (handItemMesh.VAO != 0) {
        meshSystem.deleteMesh(handItemMesh);
    }

    // Clean up textures
    textureManager.deleteTextures();
    glDeleteTextures(1, &fontTexture);

    // Clean up UI buffers
    glDeleteVertexArrays(1, &cursorVAO);
    glDeleteBuffers(1, &cursorVBO);
    glDeleteVertexArrays(1, &textVAO);
    glDeleteBuffers(1, &textVBO);
    glDeleteVertexArrays(1, &slotVAO);
    glDeleteBuffers(1, &slotVBO);
}

void RenderSystem::saveWorld(){
    {
        auto& chunkMap = world->chunkMap;

        std::lock_guard<std::mutex> lock(chunkMapMutex);
        for (auto& [hash, chunk] : chunkMap) {
            writeChunkToFile(chunk, world->seed);
        }
    }
}

void RenderSystem::generate3DCubeMesh() {
    if (handItemMesh.VAO != 0) {
        meshSystem.deleteMesh(handItemMesh);
    }

    // Light levels
    uint8_t sideLight   = 200;
    uint8_t bottomLight = 150;
    uint8_t topLight    = 255;

    std::array<glm::vec2, 4> unitUV = {
        glm::vec2(0.0f, 1.0f),
        glm::vec2(1.0f, 1.0f),
        glm::vec2(1.0f, 0.0f),
        glm::vec2(0.0f, 0.0f)
    };

    auto addFace = [&](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, 
                    int faceXOffset, uint8_t light, std::vector<Vertex>& out) {
        out.push_back({p0, unitUV[0] + glm::vec2(faceXOffset, 0), light});
        out.push_back({p1, unitUV[1] + glm::vec2(faceXOffset, 0), light});
        out.push_back({p2, unitUV[2] + glm::vec2(faceXOffset, 0), light});
        out.push_back({p3, unitUV[3] + glm::vec2(faceXOffset, 0), light});
    };

    std::vector<Vertex> vertices;
    vertices.reserve(24);

    // Back (-Z), Front (+Z), Left (-X), Right (+X) → side texture (xOffset = 0)
    addFace({-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, 0, sideLight, vertices);
    addFace({-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, 0, sideLight, vertices);
    addFace({-0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f}, 0, sideLight, vertices);
    addFace({ 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}, 0, sideLight, vertices);

    // Bottom (-Y) → xOffset = 1
    addFace({-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f}, 1, bottomLight, vertices);

    // Top (+Y) → xOffset = 2
    addFace({-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, 2, topLight, vertices);

    std::vector<unsigned int> indices = {
        0,1,2, 2,3,0,       // back
        4,5,6, 6,7,4,       // front
        8,10,9, 10,8,11,    // left
        12,13,14, 14,15,12, // right
        16,18,17, 18,16,19, // bottom
        20,22,21, 22,20,23  // top
    };

    MeshData handItemData;
    handItemData.vertices = std::move(vertices);
    handItemData.indices  = std::move(indices);

    handItemMesh = meshSystem.createMesh(handItemData);
}


void RenderSystem::drawHotbar() {
    glDisable(GL_CULL_FACE); // Disable culling for 2D text quads
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST); // UI should always be drawn on top

    glUseProgram(shader2D);

    glUniform1f(glGetUniformLocation(shader2D, "useTexture"), 1);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    int slotSpacing = width/36; // width/4/9
    for (int i = 0; i < 9; ++i) {
        glUseProgram(shader2D);

        float slotX = (width / 2.0f) - (4*slotSpacing) + (i * slotSpacing);
        float slotY = height - slotSpacing;

        glUniform2f(glGetUniformLocation(shader2D, "offset"), slotX, slotY);
        glUniform2f(glGetUniformLocation(shader2D, "scale"), slotSpacing/2, slotSpacing/2);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, (logicSystem->player->inventory.selectedSlot == i)?slotSelectedTextureID:slotTextureID);
        glUniform1i(glGetUniformLocation(shader2D, "texture0"), 0);

        // 4. Bind the quad's VAO
        glBindVertexArray(slotVAO);

        // 5. Draw the quad
        glDrawArrays(GL_TRIANGLES, 0, 6); // Assuming 6 vertices for a two-triangle quad

        // 6. Unbind the VAO for good practice
        glBindVertexArray(0);

        int id = logicSystem->player->inventory.items[i];
        if(id != 0){
            drawItem(slotX, slotSpacing, slotSpacing/2, id);
        }

        glDisable(GL_CULL_FACE); // Disable culling for 2D text quads
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
    }

    glUseProgram(shader3D_hud);
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    glm::mat4 proj3D = glm::perspective(App::fov, aspect, 0.01f, 1000.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader3D_hud, "projection"), 1, GL_FALSE, glm::value_ptr(proj3D));

    glUseProgram(shader);
}

void RenderSystem::drawItem(float slotCenterX, float slotCenterY, float slotSize, int itemID) {
    // Save current viewport
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    // Render the item in a larger viewport for better resolution, e.g., 4x the slot size.
    float renderSize = slotSize * 4.0f;

    // Calculate the bottom-left corner for the larger viewport, centered on the slot.
    int viewportX = static_cast<int>(slotCenterX - renderSize / 2.0f);
    int viewportY = static_cast<int>(slotCenterY - renderSize / 2.0f);

    glViewport(viewportX, viewportY, static_cast<int>(renderSize), static_cast<int>(renderSize));

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glClear(GL_DEPTH_BUFFER_BIT);

    glUseProgram(shader3D_hud);

    // --- View & Model (same as before) ---
    glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 3), glm::vec3(0,0,0), glm::vec3(0,1,0));
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, glm::radians(30.0f), glm::vec3(1,0,0));
    model = glm::rotate(model, glm::radians(45.0f), glm::vec3(0,1,0));

    glm::mat4 proj = glm::ortho(-2.0f, 2.0f, -2.0f, 2.0f, 0.1f, 100.0f);

    // Upload uniforms
    glUniformMatrix4fv(glGetUniformLocation(shader3D_hud, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(shader3D_hud, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader3D_hud, "model"), 1, GL_FALSE, glm::value_ptr(model));

    float uSize = meshSystem.blockTexSize / static_cast<float>(meshSystem.atlasWidthPixels);
    float vSize = meshSystem.blockTexSize / static_cast<float>(meshSystem.atlasHeightPixels);
    glUniform2f(glGetUniformLocation(shader3D_hud, "tileSize"), uSize, vSize);

    float yOffset = (itemID - 1) * vSize;
    glUniform2f(glGetUniformLocation(shader3D_hud, "tileOffset"), 0.0f, yOffset);

    // Draw block/item mesh here
    glBindTexture(GL_TEXTURE_2D, blocksTextureID);
    glBindVertexArray(handItemMesh.VAO);
    glDrawElements(GL_TRIANGLES, handItemMesh.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Restore viewport
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
}

void RenderSystem::setUpBuffers(){
    // ------------ CURSOR ----------------
    {
        float cursorVertices[] = {
            -5,  0,
            5,  0,
            0, -5,
            0,  5
        };

        // Set up VAO/VBO once
        glGenVertexArrays(1, &cursorVAO);
        glGenBuffers(1, &cursorVBO);
        glBindVertexArray(cursorVAO);
        glBindBuffer(GL_ARRAY_BUFFER, cursorVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cursorVertices), cursorVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
        glBindVertexArray(0);
    }

    // ----------------- TEXT SHADER --------------------
    {
        // Load TTF font file into memory
        std::ifstream fontFile("fonts/Lubrifont.ttf", std::ios::binary);
        fontBuffer = std::vector<unsigned char>((std::istreambuf_iterator<char>(fontFile)),std::istreambuf_iterator<char>());
        if (fontBuffer.empty()) {
            std::cerr << "Failed to load font file!\n";
            return;
        }

        // Bake font bitmap
        const int bitmapWidth = 512;
        const int bitmapHeight = 512;
        unsigned char bitmap[bitmapWidth * bitmapHeight];
        stbtt_BakeFontBitmap(fontBuffer.data(), 0, fontHeight, bitmap, bitmapWidth, bitmapHeight, 32, 96, cdata);

        // Upload to OpenGL
        glGenTextures(1, &fontTexture);
        glBindTexture(GL_TEXTURE_2D, fontTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, bitmapWidth, bitmapHeight, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Create VAO and VBO for text rendering
        glGenVertexArrays(1, &textVAO);
        glGenBuffers(1, &textVBO);

        glBindVertexArray(textVAO);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0); // position
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(1); // uv
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);
    }

    // --- HAND ITEM MESH ---
    generate3DCubeMesh();

    // --- SLOT MESH ---
    {
        float vertices[] = {
            // positions    // texture coords
            -1.0f, -1.0f,  0.0f, 0.0f, // bottom-left
            1.0f, -1.0f,  1.0f, 0.0f, // bottom-right
            1.0f,  1.0f,  1.0f, 1.0f, // top-right

            1.0f,  1.0f,  1.0f, 1.0f, // top-right
            -1.0f,  1.0f,  0.0f, 1.0f, // top-left
            -1.0f, -1.0f,  0.0f, 0.0f  // bottom-left
        };

        glGenVertexArrays(1, &slotVAO); // Generate a VAO
        glGenBuffers(1, &slotVBO);     // Generate a VBO

        glBindVertexArray(slotVAO);    // Bind the VAO

        glBindBuffer(GL_ARRAY_BUFFER, slotVBO); // Bind the VBO
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW); // Populate VBO with vertex data

        // Configure vertex position attribute (layout = 0)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Configure texture coordinate attribute (layout = 1)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Unbind the VBO and VAO for a clean state
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
}