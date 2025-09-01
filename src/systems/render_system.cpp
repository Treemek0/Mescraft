#include "render_system.h"
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize2.h"
#include <iostream>
#include <GL/gl.h>
#include "../controller/app.h"
#include "../world/world.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>
#include <thread>
#include <../utilities/raycast.h>

stbtt_bakedchar cdata[96];
GLuint fontTexture;
std::vector<unsigned char> fontBuffer;
int fontHeight = 32;

RenderSystem::RenderSystem(unsigned int shader, unsigned int shaderText, unsigned int shader2D, GLFWwindow* window, World* w, std::unordered_map<unsigned int, TransformComponent> transformComponents) : world(w), runningCreationThread(true) {
    this->shader = shader;
    this->shaderText = shaderText;
    this->shader2D = shader2D;
    modelLocation = glGetUniformLocation(shader, "model");
    blocksTextureID = make_texture("textures/blocks.png");
    hoverTextureID = make_texture("textures/hover.png");
    this->window = window;

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
    
void RenderSystem::update(std::unordered_map<unsigned int, TransformComponent> &transformComponents, std::unordered_map<unsigned int, RenderComponent> &renderComponents) {

    // if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
    //     glm::vec3 playerPos = transformComponents[App::cameraID].position;
        
    //     int playerChunkX = static_cast<int>(floor(playerPos.x / CHUNK_SIZE));
    //     int playerChunkY = static_cast<int>(floor(playerPos.y / CHUNK_SIZE));
    //     int playerChunkZ = static_cast<int>(floor(playerPos.z / CHUNK_SIZE));

    //     auto hash = hashChunkCoords(playerChunkX, playerChunkY, playerChunkZ);

    //     std::scoped_lock lock(chunkMapMutex, meshCreationQueueMutex);
    //     Chunk& chunk = world->chunkMap.at(hash);
    //     Mesh& mesh = chunksMesh.at(hash);
    //     int localX = static_cast<int>(floor(playerPos.x) - playerChunkX * CHUNK_SIZE);
    //     int localY = static_cast<int>(floor(playerPos.y - 2) - playerChunkY * CHUNK_SIZE);
    //     int localZ = static_cast<int>(floor(playerPos.z) - playerChunkZ * CHUNK_SIZE);

    //     setBlockID(chunk, localX, localY, localZ, 3);

    //     meshSystem.updateMeshDataWithBlock(chunksMesh[hash], chunk, world->chunkMap, localX, localY, localZ);
    // }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    {
        std::lock_guard<std::mutex> lock(cameraMutex);
        if(transformComponents.count(App::cameraID)){
            cameraPosForThread = transformComponents[App::cameraID].position;
        }
    }

    processMeshQueue(); 

    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));

    for (auto& pair : chunksMesh) {
        Mesh& mesh = pair.second;
        glBindVertexArray(mesh.VAO);
        glBindTexture(GL_TEXTURE_2D, blocksTextureID);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
    }
    
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

    drawCursor();

	glfwSwapBuffers(window);
}

void RenderSystem::processMeshQueue() {
    // Process new meshes to be created (OpenGL calls on main thread)
    std::lock_guard<std::mutex> meshLock(meshQueueMutex);
    for (auto& [hash, meshData] : meshQueue) {
        Mesh mesh = meshSystem.createMesh(meshData); // OpenGL call on main thread
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

unsigned int RenderSystem::make_texture(const char* filename) {
    int width, height, channels;
	unsigned char* data = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);
    if (data == NULL) {
        std::cout << "Failed to load texture: " << filename << std::endl;
        return 0;
    }
    
	//make the texture
    unsigned int texture;
	glGenTextures(1, &texture);
    textures.push_back(texture);
    glBindTexture(GL_TEXTURE_2D, texture);
	
    //load data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    // Generate mipmaps for smoother textures at a distance
    glGenerateMipmap(GL_TEXTURE_2D);

    //free data
	stbi_image_free(data);

    //Configure sampler
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return texture;
}


void RenderSystem::drawText(const std::string& text, float x, float y, float scale, const glm::vec3& color) 
{
     // --- State setup for 2D rendering ---
    glDisable(GL_CULL_FACE); // Disable culling for 2D text quads
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST); // UI should always be drawn on top

    glUseProgram(shaderText);

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderText, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

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

double placeLastTime = glfwGetTime();
void RenderSystem::renderHoverBlock(glm::vec3 playerPos, glm::vec3 cameraDir, float eyeHeight){
    RaycastHit hit = raycast(playerPos + glm::vec3(0, eyeHeight, 0), cameraDir, 5.0f, world->chunkMap);

    if (hit.hit) {
        int x = hit.block.position.x;
        int y = hit.block.position.y;
        int z = hit.block.position.z;

        float offset = 0.001f;
        float layer = 4.0F; // 4 - full light on block

        float vertices[] = {
            // Front (+Z)
            x-offset, y-offset, z+1+offset, 0.0f, 0.0f, layer,
            x+1+offset, y-offset, z+1+offset, 1.0f, 0.0f, layer,
            x+1+offset, y+1+offset, z+1+offset, 1.0f, 1.0f, layer,
            x-offset, y+1+offset, z+1+offset, 0.0f, 1.0f, layer,

            // Back (-Z)
            x+1+offset, y-offset, z-offset, 0.0f, 0.0f, layer,
            x-offset, y-offset, z-offset, 1.0f, 0.0f, layer,
            x-offset, y+1+offset, z-offset, 1.0f, 1.0f, layer,
            x+1+offset, y+1+offset, z-offset, 0.0f, 1.0f, layer,

            // Left (-X)
            x-offset, y-offset, z-offset, 0.0f, 0.0f, layer,
            x-offset, y-offset, z+1+offset, 1.0f, 0.0f, layer,
            x-offset, y+1+offset, z+1+offset, 1.0f, 1.0f, layer,
            x-offset, y+1+offset, z-offset, 0.0f, 1.0f, layer,

            // Right (+X)
            x+1+offset, y-offset, z+1+offset, 0.0f, 0.0f, layer,
            x+1+offset, y-offset, z-offset, 1.0f, 0.0f, layer,
            x+1+offset, y+1+offset, z-offset, 1.0f, 1.0f, layer,
            x+1+offset, y+1+offset, z+1+offset, 0.0f, 1.0f, layer,

            // Top (+Y)
            x-offset, y+1+offset, z-offset, 0.0f, 0.0f, layer,
            x+1+offset, y+1+offset, z-offset, 1.0f, 0.0f, layer,
            x+1+offset, y+1+offset, z+1+offset, 1.0f, 1.0f, layer,
            x-offset, y+1+offset, z+1+offset, 0.0f, 1.0f, layer,

            // Bottom (-Y)
            x-offset, y-offset, z+1+offset, 0.0f, 0.0f, layer,
            x+1+offset, y-offset, z+1+offset, 1.0f, 0.0f, layer,
            x+1+offset, y-offset, z-offset, 1.0f, 1.0f, layer,
            x-offset, y-offset, z-offset, 0.0f, 1.0f, layer
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
        data.vertices.assign(vertices, vertices + sizeof(vertices)/sizeof(float));
        data.indices.assign(indices, indices + sizeof(indices)/sizeof(unsigned int));

        Mesh hoverMesh = meshSystem.createMesh(data);

        glEnable(GL_BLEND);
        glBindVertexArray(hoverMesh.VAO);
        glBindTexture(GL_TEXTURE_2D, hoverTextureID);
        glDrawElements(GL_TRIANGLES, hoverMesh.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glDisable(GL_BLEND);


        // breaking and placing blocks
        try
        {
            if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS && glfwGetTime() - placeLastTime > 0.2) {
                placeLastTime = glfwGetTime();

                std::scoped_lock lock(chunkMapMutex, meshCreationQueueMutex);
                int wx = x + hit.faceNormal.x;
                int wy = y + hit.faceNormal.y;
                int wz = z + hit.faceNormal.z;

                int cx = wx / CHUNK_SIZE;
                if (wx < 0 && wx % CHUNK_SIZE != 0) --cx;
                int cy = wy / CHUNK_SIZE;
                if (wy < 0 && wy % CHUNK_SIZE != 0) --cy;
                int cz = wz / CHUNK_SIZE;
                if (wz < 0 && wz % CHUNK_SIZE != 0) --cz;

                uint64_t hash = hashChunkCoords(cx, cy, cz);
                Chunk& chunk = world->chunkMap[hash];

                // Local coordinates inside the chunk
                int localX = wx - cx * CHUNK_SIZE;
                int localY = wy - cy * CHUNK_SIZE;
                int localZ = wz - cz * CHUNK_SIZE;
                
                changeBlockID(chunk, localX, localY, localZ, 3);

                auto data = meshSystem.createChunkData(chunk, world->chunkMap);
                chunksMesh[hash] = meshSystem.createMesh(data);
            }

            if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && glfwGetTime() - placeLastTime > 0.2) {
                placeLastTime = glfwGetTime();

                std::scoped_lock lock(chunkMapMutex, meshCreationQueueMutex);

                int cx = x / CHUNK_SIZE;
                if (x < 0 && x % CHUNK_SIZE != 0) --cx;
                int cy = y / CHUNK_SIZE;
                if (y < 0 && y % CHUNK_SIZE != 0) --cy;
                int cz = z / CHUNK_SIZE;
                if (z < 0 && z % CHUNK_SIZE != 0) --cz;

                uint64_t hash = hashChunkCoords(cx, cy, cz);
                Chunk& chunk = world->chunkMap[hash];

                // Local coordinates inside the chunk
                int localX = x - cx * CHUNK_SIZE;
                int localY = y - cy * CHUNK_SIZE;
                int localZ = z - cz * CHUNK_SIZE;
                
                changeBlockID(chunk, localX, localY, localZ, 0);

                auto data = meshSystem.createChunkData(chunk, world->chunkMap);
                chunksMesh[hash] = meshSystem.createMesh(data);

                if(localX == 0){
                    uint64_t hash = hashChunkCoords(cx - 1, cy, cz);
                    Chunk& chunk = world->chunkMap[hash];

                    auto data = meshSystem.createChunkData(chunk, world->chunkMap);
                    chunksMesh[hash] = meshSystem.createMesh(data);
                }else if(localX == CHUNK_SIZE - 1){
                    uint64_t hash = hashChunkCoords(cx + 1, cy, cz);
                    Chunk& chunk = world->chunkMap[hash];

                    auto data = meshSystem.createChunkData(chunk, world->chunkMap);
                    chunksMesh[hash] = meshSystem.createMesh(data);
                }
                
                if(localY == 0){
                    uint64_t hash = hashChunkCoords(cx, cy - 1, cz);
                    Chunk& chunk = world->chunkMap[hash];

                    auto data = meshSystem.createChunkData(chunk, world->chunkMap);
                    chunksMesh[hash] = meshSystem.createMesh(data);
                }else if(localY == CHUNK_SIZE - 1){
                    uint64_t hash = hashChunkCoords(cx, cy + 1, cz);
                    Chunk& chunk = world->chunkMap[hash];

                    auto data = meshSystem.createChunkData(chunk, world->chunkMap);
                    chunksMesh[hash] = meshSystem.createMesh(data);
                }
                
                if(localZ == 0){
                    uint64_t hash = hashChunkCoords(cx, cy, cz - 1);
                    Chunk& chunk = world->chunkMap[hash];

                    auto data = meshSystem.createChunkData(chunk, world->chunkMap);
                    chunksMesh[hash] = meshSystem.createMesh(data);
                }else if(localZ == CHUNK_SIZE - 1){
                    uint64_t hash = hashChunkCoords(cx, cy, cz + 1);
                    Chunk& chunk = world->chunkMap[hash];

                    auto data = meshSystem.createChunkData(chunk, world->chunkMap);
                    chunksMesh[hash] = meshSystem.createMesh(data);
                }
            }
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
        
        
    }
}

void RenderSystem::drawCursor() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    glDisable(GL_CULL_FACE); // Disable culling for 2D text quads
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST); // UI should always be drawn on top

    glUseProgram(shader2D);

    glm::mat4 projection = glm::ortho(0.0f, float(width), float(height), 0.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader2D, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glUniform1i(glGetUniformLocation(shader2D, "useTexture"), 0);  // plain color
    glUniform4f(glGetUniformLocation(shader2D, "color"), 1.0f, 1.0f, 1.0f, 1.0f); // white

    float cursorSize = 0.5f;
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

RenderSystem::~RenderSystem() {
    runningCreationThread = false; // Signal the worker thread to stop
    if (dataCreationThread.joinable()) {
        dataCreationThread.join(); // Wait for the worker thread to finish
    }

    chunksToDeleteQueue.clear();
    meshQueue.clear();
    chunksMesh.clear();
}

void RenderSystem::setUpBuffers(){
    // ------------ CURSOR ----------------

    float cursorVertices[] = {
        -15,  0,
        15,  0,
        0, -15,
        0,  15
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


    // ----------------- TEXT SHADER --------------------

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

void RenderSystem::saveWorld(){
    {
        auto& chunkMap = world->chunkMap;

        std::lock_guard<std::mutex> lock(chunkMapMutex);
        for (auto& [hash, chunk] : chunkMap) {
            writeChunkToFile(chunk, world->seed);
        }
    }
}