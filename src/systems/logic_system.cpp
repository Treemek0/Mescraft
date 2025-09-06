#include "logic_system.h"
#include "render_system.h"

LogicSystem::LogicSystem(GLFWwindow* window, World* w) : left_click(GLFW_MOUSE_BUTTON_LEFT, true, 1, window, 0.2f), right_click(GLFW_MOUSE_BUTTON_RIGHT, true, 1, window, 0.2f), middle_click(GLFW_MOUSE_BUTTON_MIDDLE, false, 1, window) {
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
    }

    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        player->inventory.selectedSlot = 1;
    }

    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
        player->inventory.selectedSlot = 2;
    }

    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) {
        player->inventory.selectedSlot = 3;
    }

    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) {
        player->inventory.selectedSlot = 4;
    }
}

void LogicSystem::handlePlayerMouseClick(RaycastHit hit, std::mutex& chunkMapMutex, std::mutex& meshCreationQueueMutex, MeshSystem& meshSystem, std::unordered_map<uint64_t, Mesh>& chunksMesh){
    try
        {
            int x = hit.block.position.x;
            int y = hit.block.position.y;
            int z = hit.block.position.z;

            if(middle_click.isPressed()){
                std::scoped_lock lock(chunkMapMutex);
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

                int id = static_cast<int>(getBlockID(chunk, localX, localY, localZ));

                std::cout << "Scroll click, id:" << id << std::endl;
                std::cout << localX << " " << localY << " " << localZ << std::endl;

                bool foundGoodSlot = false;
                for (int i = 0; i < 9; i++) {
                    if (player->inventory.items[i] == id) {
                        std::cout << "Found item in inventory at slot: " << i << std::endl;
                        player->inventory.selectedSlot = i;
                        foundGoodSlot = true;
                        break;
                    }

                    if(player->inventory.items[i] == 0){
                        std::cout << "Found empty slot in inventory at slot: " << i << std::endl;
                        player->inventory.items[i] = id;
                        player->inventory.selectedSlot = i;
                        foundGoodSlot = true;
                        break;
                    }
                }

                if(!foundGoodSlot){
                    player->inventory.items[player->inventory.selectedSlot] = id;
                }
            }

            if(right_click.isPressed()) {
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
                Mesh mesh = meshSystem.createMesh(data);

                mesh.startPositonOfChunk = {cx * CHUNK_SIZE, cy * CHUNK_SIZE, cz * CHUNK_SIZE};

                chunksMesh[hash] = mesh;
            }

            if(left_click.isPressed()) {
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
                Mesh mesh = meshSystem.createMesh(data);

                mesh.startPositonOfChunk = {cx * CHUNK_SIZE, cy * CHUNK_SIZE, cz * CHUNK_SIZE};

                chunksMesh[hash] = mesh;

                if(localX == 0){
                    uint64_t hash = hashChunkCoords(cx - 1, cy, cz);
                    Chunk& chunk = world->chunkMap[hash];

                    auto data = meshSystem.createChunkData(chunk, world->chunkMap);
                    Mesh mesh = meshSystem.createMesh(data);

                    mesh.startPositonOfChunk = {(cx - 1) * CHUNK_SIZE, cy * CHUNK_SIZE, cz * CHUNK_SIZE};

                    chunksMesh[hash] = mesh;
                }else if(localX == CHUNK_SIZE - 1){
                    uint64_t hash = hashChunkCoords(cx + 1, cy, cz);
                    Chunk& chunk = world->chunkMap[hash];

                    auto data = meshSystem.createChunkData(chunk, world->chunkMap);
                    Mesh mesh = meshSystem.createMesh(data);

                    mesh.startPositonOfChunk = {(cx + 1) * CHUNK_SIZE, cy * CHUNK_SIZE, cz * CHUNK_SIZE};

                    chunksMesh[hash] = mesh;
                }
                
                if(localY == 0){
                    uint64_t hash = hashChunkCoords(cx, cy - 1, cz);
                    Chunk& chunk = world->chunkMap[hash];

                    auto data = meshSystem.createChunkData(chunk, world->chunkMap);
                    Mesh mesh = meshSystem.createMesh(data);

                    mesh.startPositonOfChunk = {cx * CHUNK_SIZE, (cy - 1) * CHUNK_SIZE, cz * CHUNK_SIZE};

                    chunksMesh[hash] = mesh;
                }else if(localY == CHUNK_SIZE - 1){
                    uint64_t hash = hashChunkCoords(cx, cy + 1, cz);
                    Chunk& chunk = world->chunkMap[hash];

                    auto data = meshSystem.createChunkData(chunk, world->chunkMap);
                    Mesh mesh = meshSystem.createMesh(data);

                    mesh.startPositonOfChunk = {cx * CHUNK_SIZE, (cy + 1) * CHUNK_SIZE, cz * CHUNK_SIZE};

                    chunksMesh[hash] = mesh;
                }
                
                if(localZ == 0){
                    uint64_t hash = hashChunkCoords(cx, cy, cz - 1);
                    Chunk& chunk = world->chunkMap[hash];

                    auto data = meshSystem.createChunkData(chunk, world->chunkMap);
                    Mesh mesh = meshSystem.createMesh(data);

                    mesh.startPositonOfChunk = {cx * CHUNK_SIZE, cy * CHUNK_SIZE, (cz - 1) * CHUNK_SIZE};

                    chunksMesh[hash] = mesh;
                }else if(localZ == CHUNK_SIZE - 1){
                    uint64_t hash = hashChunkCoords(cx, cy, cz + 1);
                    Chunk& chunk = world->chunkMap[hash];

                    auto data = meshSystem.createChunkData(chunk, world->chunkMap);
                    Mesh mesh = meshSystem.createMesh(data);

                    mesh.startPositonOfChunk = {cx * CHUNK_SIZE, cy * CHUNK_SIZE, (cz + 1) * CHUNK_SIZE};
                    
                    chunksMesh[hash] = mesh;
                }
            }
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
}

void LogicSystem::scroll(double xoffset, double yoffset){
    if(yoffset < 0){
        player->inventory.selectedSlot++;
    }else{
        player->inventory.selectedSlot--;
    }

    if(player->inventory.selectedSlot < 0){
        player->inventory.selectedSlot = 8;
    }else if(player->inventory.selectedSlot > 8){
        player->inventory.selectedSlot = 0;
    }
}
