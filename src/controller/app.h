#pragma once
#include "../components/camera_component.h"
#include "../components/physics_component.h"
#include "../components/render_component.h"
#include "../components/transform_component.h"

#include "../systems/camera_system.h"
#include "../systems/physics_system.h"
#include "../systems/render_system.h"
#include "../systems/mesh_system.h"

#include "../shaders/shader.h"
#include "../entities/player.h"

#include "../world/world.h"
#include <logic_system.h>


class App {
public:
    App();
    ~App();
    void run();
    unsigned int make_entity(glm::vec3 position, glm::vec3 rotation, glm::vec3 velocity);
    Mesh make_cube_mesh(glm::vec3 size);
    void set_up_opengl();
    void make_systems();

    static unsigned int shaders[4];

    static float fov;

    static size_t fpsCounter;
    static size_t fps;

    //Components
    std::unordered_map<unsigned int, TransformComponent> transformComponents;
    std::unordered_map<unsigned int, PhysicsComponent> physicsComponents;
    CameraComponent* cameraComponent;
    Player* player;
    static unsigned int cameraID;
    std::unordered_map<unsigned int, RenderComponent> renderComponents;
    
    //Systems
    MotionSystem* motionSystem;
    CameraSystem* cameraSystem;
    RenderSystem* renderSystem;
    MeshSystem* meshSystem;
    LogicSystem* logicSystem;

    World* world;

private:
    void set_up_glfw();
    static void window_size_changed_callback(GLFWwindow* window, int width, int height);

    unsigned int entity_count = 0;
    GLFWwindow* window;
};