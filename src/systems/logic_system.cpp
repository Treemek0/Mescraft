#include "logic_system.h"
#include "render_system.h"

LogicSystem::LogicSystem(GLFWwindow* window, World* w) {
    player = new Player();
    this->window = window;
    this->world = w;

    player->inventory.items[0] = 1;
    player->inventory.items[1] = 2;
    player->inventory.items[2] = 3;
    player->inventory.items[3] = 4;
    player->inventory.items[4] = 5;
    player->inventory.selectedSlot = 0;
};

LogicSystem::~LogicSystem() noexcept {
    delete player;
};

void LogicSystem::update(float dt){
    player->update(dt);
}

void LogicSystem::updatePlayerSlotKeys(RenderSystem* renderSystem){
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
        player->inventory.selectedSlot = 0;
        renderSystem->generateHandItemMesh(player->inventory.items[player->inventory.selectedSlot]);
    }

    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        player->inventory.selectedSlot = 1;
        renderSystem->generateHandItemMesh(player->inventory.items[player->inventory.selectedSlot]);
    }

    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
        player->inventory.selectedSlot = 2;
        renderSystem->generateHandItemMesh(player->inventory.items[player->inventory.selectedSlot]);
    }

    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) {
        player->inventory.selectedSlot = 3;
        renderSystem->generateHandItemMesh(player->inventory.items[player->inventory.selectedSlot]);
    }

    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) {
        player->inventory.selectedSlot = 4;
        renderSystem->generateHandItemMesh(player->inventory.items[player->inventory.selectedSlot]);
    }
}

double placeLastTime = glfwGetTime();
void LogicSystem::handlePlayerMouseClick(RaycastHit hit, std::mutex& chunkMapMutex, std::mutex& meshCreationQueueMutex, MeshSystem& meshSystem, std::unordered_map<uint64_t, Mesh>& chunksMesh){
    try
        {
            int x = hit.block.position.x;
            int y = hit.block.position.y;
            int z = hit.block.position.z;

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
                
                changeBlockID(chunk, localX, localY, localZ, player->inventory.getSelectedItem());

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
