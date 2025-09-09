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

    logicSystem->renderSystem = this;

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
            processChunkGeneration(); 
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
    
    logicSystem->updatePlayerSlotKeys();

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
    drawText(coordinates, 5.0f, 0.0f, 1.0f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    drawText(std::to_string(App::fps), 5.0f, fontHeight, 1.0f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    
    drawHandItem(transformComponents);
    drawHotbar();
    drawCursor();

    {
        float elapsed = glfwGetTime() - itemNameTextShowTime;
        if(elapsed < 1.5f){
            int slotSpacing = width/36; // width/4/9
            double scale = getScaleForHeight(slotSpacing/2);
            double textWidth = getTextWidth(itemNameText, scale);
            double textHeight = getTextHeight(scale);
            
            float alpha = 0.0f;

            if (elapsed > 1.25f) {
                // Fade out (1 → 0)
                alpha = (1.5f - elapsed) / 0.25f;
            } else {
                // Fully visible
                alpha = 1.0f;
            }
            
            drawText(itemNameText, width/2 - textWidth/2, height - (slotSpacing * 1.5) - (textHeight*1.5), scale, glm::vec4(1.0f, 1.0f, 1.0f, alpha));
        }
    }

	glfwSwapBuffers(window);
}

void RenderSystem::processMeshQueue() {
    std::lock_guard<std::mutex> meshLock(meshQueueMutex);
    for (auto& [hash, meshData] : meshQueue) {
        Mesh mesh = meshSystem.createMesh(meshData);

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
            {
                std::unique_lock lock(world->loadedChunksMutex);
                world->loadedChunks.erase(hash);
            }
        }
    }
    chunksToDeleteQueue.clear();
}

void RenderSystem::processChunkGeneration() {
    uint64_t hash_to_process = 0;
    bool job_found = false;

    // --- Find a chunk to generate ---
    {
        std::lock_guard<std::mutex> lock(cameraMutex);
        if (!chunksToGenerate.empty()) {
            hash_to_process = chunksToGenerate.back();
            chunksToGenerate.pop_back();
            job_found = true;
        }
    }

    if (!job_found) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // no work
        return;
    }

    // --- Process the chunk ---
    int cx, cy, cz;
    decodeChunkHash(hash_to_process, cx, cy, cz);

    auto chunk = std::make_shared<Chunk>();
    chunk->position = {
        static_cast<float>(cx * CHUNK_SIZE),
        static_cast<float>(cy * CHUNK_SIZE),
        static_cast<float>(cz * CHUNK_SIZE)
    };

    world->generateChunk(*chunk); 

    {
        std::unique_lock lock(world->loadedChunksMutex);
        world->loadedChunks.insert(hash_to_process);
    }
    {
        std::unique_lock lock(world->chunkMapMutex);
        world->chunkMap.emplace(hash_to_process, chunk);
    }
    {
        std::scoped_lock lock(meshCreationQueueMutex);
        meshCreationQueue.emplace(hash_to_process, chunk);
    }
}

void RenderSystem::generate_world(const glm::vec3& playerPos) {
    auto& chunkMap = world->chunkMap;

    int playerChunkX = static_cast<int>(floor(playerPos.x / CHUNK_SIZE));
    int playerChunkY = static_cast<int>(floor(playerPos.y / CHUNK_SIZE));
    int playerChunkZ = static_cast<int>(floor(playerPos.z / CHUNK_SIZE));

    // only update chunks if the player has moved to a new chunk
    if (playerChunkX == lastPlayerChunkX && playerChunkY == lastPlayerChunkY && playerChunkZ == lastPlayerChunkZ) {
        return;
    }

    lastPlayerChunkX = playerChunkX;
    lastPlayerChunkY = playerChunkY;
    lastPlayerChunkZ = playerChunkZ;

    int render_dist_h = RENDER_DISTANCE / 2;
    int render_dist_v = VERTICAL_RENDER_DISTANCE / 2;

    const int load_dist_h = render_dist_h + 1;
    const int load_dist_v = render_dist_v + 1;

    std::vector<uint64_t> new_chunks_to_generate;
    // --- 1. Unload distant chunks ---
    {
        std::unique_lock lock(world->chunkMapMutex);
        for (auto it = chunkMap.begin(); it != chunkMap.end(); ) {
            int cx, cy, cz;
            decodeChunkHash(it->first, cx, cy, cz);

            if (abs(cx - playerChunkX) > load_dist_h || abs(cy - playerChunkY) > load_dist_v || abs(cz - playerChunkZ) > load_dist_h) {
                {
                    std::lock_guard<std::mutex> lock(meshDeleteQueueMutex);
                    chunksToDeleteQueue.push_back(it->first);
                }

                writeChunkToFile(*(it->second), world->seed);
                it = chunkMap.erase(it);
            } else {
                ++it;
            }
        }
    }

    // --- 2. Discover new chunks to load ---
    for (int dx = -load_dist_h; dx <= load_dist_h; ++dx) {
        for (int dz = -load_dist_h; dz <= load_dist_h; ++dz) {
            for (int dy = -load_dist_v; dy <= load_dist_v; ++dy) {
                int cx = playerChunkX + dx;
                int cy = playerChunkY + dy;
                int cz = playerChunkZ + dz;
                uint64_t hash = hashChunkCoords(cx, cy, cz);

                bool chunk_exists;
                {
                    std::shared_lock lock(world->chunkMapMutex);
                    chunk_exists = (chunkMap.count(hash) > 0);
                }

                if (!chunk_exists) {
                    new_chunks_to_generate.push_back(hash);
                }
            }
        }
    }

    // --- 3. Prioritize and queue the new chunks ---
    if (!new_chunks_to_generate.empty()) {
        std::sort(new_chunks_to_generate.begin(), new_chunks_to_generate.end(),
            [&](uint64_t a, uint64_t b) {
                int ax, ay, az, bx, by, bz;
                decodeChunkHash(a, ax, ay, az);
                decodeChunkHash(b, bx, by, bz);
                // Sort by distance from player
                return (abs(ax - playerChunkX) + abs(az - playerChunkZ) + abs(ay - playerChunkY)) >
                       (abs(bx - playerChunkX) + abs(bz - playerChunkZ) + abs(by - playerChunkY));
            });

        std::lock_guard<std::mutex> lock(cameraMutex);
        chunksToGenerate.insert(chunksToGenerate.end(), new_chunks_to_generate.begin(), new_chunks_to_generate.end());
    }
}


const int neighborOffsets[6][3] = {
    {1, 0, 0}, {-1, 0, 0},
    {0, 1, 0}, {0, -1, 0},
    {0, 0, 1}, {0, 0, -1}
};

void RenderSystem::generate_world_meshes() {
    std::vector<std::pair<uint64_t, std::shared_ptr<Chunk>>> ready_to_mesh;

    {
        std::shared_lock lock(meshCreationQueueMutex);
        if (meshCreationQueue.empty()) {
            return;
        }

        ready_to_mesh.reserve(meshCreationQueue.size());
        for (const auto& pair : meshCreationQueue) {
            bool is_ready;
            {
                std::shared_lock loaded_lock(world->loadedChunksMutex);
                is_ready = neighborsReady(pair.first, world->loadedChunks);
            }
            if (is_ready) {
                ready_to_mesh.push_back(pair);
            }
        }
    }

    if (ready_to_mesh.empty()) {
        return;
    }

    for (const auto& pair : ready_to_mesh) {
        const auto& hash = pair.first;
        const auto& chunkToMesh = pair.second;

        // Gather neighbor data under a single lock to avoid repeated locking inside createChunkData.
        std::array<std::shared_ptr<Chunk>, 6> neighbors;
        {
            std::shared_lock chunkLock(world->chunkMapMutex);
            int cx, cy, cz;
            decodeChunkHash(hash, cx, cy, cz);
            for (int i = 0; i < 6; ++i) {
                uint64_t nHash = hashChunkCoords(cx + neighborOffsets[i][0], cy + neighborOffsets[i][1], cz + neighborOffsets[i][2]);
                auto it = world->chunkMap.find(nHash);
                neighbors[i] = (it != world->chunkMap.end()) ? it->second : nullptr;
            }
        }

        MeshData meshData = meshSystem.createChunkData(*chunkToMesh, neighbors);
        
        std::lock_guard<std::mutex> meshLock(meshQueueMutex);
        meshQueue.push_back({hash, meshData});
    }

    {
        std::unique_lock lock(meshCreationQueueMutex);
        for (const auto& pair : ready_to_mesh) {
            meshCreationQueue.erase(pair.first);
        }
    }
}

bool RenderSystem::neighborsReady(uint64_t hash, const std::unordered_set<uint64_t>& loadedChunks) {
    int cx, cy, cz;
    decodeChunkHash(hash, cx, cy, cz);

    for (auto& offset : neighborOffsets) {
        uint64_t nHash = hashChunkCoords(cx + offset[0], cy + offset[1], cz + offset[2]);
        if (!loadedChunks.count(nHash)) return false;
    }
    return true;
}


void RenderSystem::drawText(const std::string& text, float x, float y, float scale, const glm::vec4& color) 
{
     // --- Setup for 2D rendering ---
    glDisable(GL_CULL_FACE); 
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST); // UI drawn on top

    y /= scale;
    x /= scale;

    glUseProgram(shaderText);

    // Set text color
    glUniform4f(glGetUniformLocation(shaderText, "textColor"), color.r, color.g, color.b, color.a);

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

        float offset = 0.005f;
        uint8_t layer = 255; // 255 - full light on block

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
        if(hit.distance == 0){ // inside block
            glDisable(GL_CULL_FACE); 
        }

        glEnable(GL_BLEND);
        glBindVertexArray(hoverMesh.VAO);
        glBindTexture(GL_TEXTURE_2D, hoverTextureID);
        glDrawElements(GL_TRIANGLES, hoverMesh.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        meshSystem.deleteMesh(hoverMesh);

        // breaking and placing blocks & middle clicking
        logicSystem->handlePlayerMouseClick(hit, world->chunkMapMutex, meshCreationQueueMutex, meshSystem, chunksMesh);
    }
}

void RenderSystem::drawCursor() {
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

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
    if(logicSystem->player->inventory.getSelectedItem().id == 0) return;

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

    int itemID = logicSystem->player->inventory.getSelectedItem().id;
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

    glUseProgram(shader);
}

RenderSystem::~RenderSystem() {
    runningCreationThread = false;
    if (dataCreationThread.joinable()) {
        dataCreationThread.join();
    }
    if (meshCreationThread.joinable()) {
        meshCreationThread.join();
    }

    // Clean up all chunk meshes
    for (auto& pair : chunksMesh) {
        meshSystem.deleteMesh(pair.second);
    }
    chunksMesh.clear();

    if (handItemMesh.VAO != 0) {
        meshSystem.deleteMesh(handItemMesh);
    }

    textureManager.deleteTextures();
    glDeleteTextures(1, &fontTexture);

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

        std::shared_lock chunkLock(world->chunkMapMutex);
        for (auto& [hash, chunk] : chunkMap) {
            writeChunkToFile(*chunk, world->seed);
        }
    }
}

void RenderSystem::generate3DCubeMesh() {
    if (handItemMesh.VAO != 0) {
        meshSystem.deleteMesh(handItemMesh);
    }

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
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST); 

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

        glBindVertexArray(slotVAO);

        glDrawArrays(GL_TRIANGLES, 0, 6); 

        glBindVertexArray(0);

        int id = logicSystem->player->inventory.items[i].id;
        if(id != 0){
            drawItem(slotX, slotSpacing, slotSpacing/2, id);
        }

        glDisable(GL_CULL_FACE);
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
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    // render the item in a larger viewport for better resolution
    float renderSize = slotSize * 4.0f;

    // bottom-left corner for the larger viewport, centered on the slot.
    int viewportX = static_cast<int>(slotCenterX - renderSize / 2.0f);
    int viewportY = static_cast<int>(slotCenterY - renderSize / 2.0f);

    glViewport(viewportX, viewportY, static_cast<int>(renderSize), static_cast<int>(renderSize));

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glClear(GL_DEPTH_BUFFER_BIT);

    glUseProgram(shader3D_hud);

    glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 3), glm::vec3(0,0,0), glm::vec3(0,1,0));
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, glm::radians(30.0f), glm::vec3(1,0,0));
    model = glm::rotate(model, glm::radians(45.0f), glm::vec3(0,1,0));

    glm::mat4 proj = glm::ortho(-2.0f, 2.0f, -2.0f, 2.0f, 0.1f, 100.0f);

    glUniformMatrix4fv(glGetUniformLocation(shader3D_hud, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(shader3D_hud, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader3D_hud, "model"), 1, GL_FALSE, glm::value_ptr(model));

    float uSize = meshSystem.blockTexSize / static_cast<float>(meshSystem.atlasWidthPixels);
    float vSize = meshSystem.blockTexSize / static_cast<float>(meshSystem.atlasHeightPixels);
    glUniform2f(glGetUniformLocation(shader3D_hud, "tileSize"), uSize, vSize);

    float yOffset = (itemID - 1) * vSize;
    glUniform2f(glGetUniformLocation(shader3D_hud, "tileOffset"), 0.0f, yOffset);

    // draw item mesh
    glBindTexture(GL_TEXTURE_2D, blocksTextureID);
    glBindVertexArray(handItemMesh.VAO);
    glDrawElements(GL_TRIANGLES, handItemMesh.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // restore viewport
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

        // vertex position attribute (layout = 0)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // texture coordinate attribute (layout = 1)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Unbind the VBO and VAO
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
}

void RenderSystem::drawHotbarText(std::string text){
    itemNameTextShowTime = glfwGetTime();
    itemNameText = text;
}

float RenderSystem::getTextWidth(const std::string& text, float scale){
    float x = 0.0f;
    float y = 0.0f;

    for (const char* p = text.c_str(); *p; p++)
    {
        if (*p >= 32 && *p < 128)
        {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(cdata, 512, 512, *p - 32, &x, &y, &q, 1);
        }
    }

    return x * scale;
}

float RenderSystem::getScaleForHeight(float desiredHeight){
    return desiredHeight / fontHeight;
}

float RenderSystem::getTextHeight(float scale){
    return fontHeight * scale;
}