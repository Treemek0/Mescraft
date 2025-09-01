#include "app.h"

float App::fov = 90.0f;
size_t App::fps = 0;
size_t App::fpsCounter = 0;
unsigned int App::cameraID = 0;

App::App() {
    set_up_glfw();
}

App::~App() {
    glDeleteTextures(renderSystem->textures.size(), renderSystem->textures.data());
    glDeleteProgram(shader3D);
    glDeleteProgram(shaderText);
    glDeleteProgram(shader2D);

    renderSystem->saveWorld();

    delete cameraComponent;
    delete motionSystem;
    delete cameraSystem;
    delete renderSystem;
    delete meshSystem;
    delete world;
    
    glfwTerminate();
}

unsigned int App::make_entity(glm::vec3 position, glm::vec3 rotation, glm::vec3 velocity) {
    int entity = entity_count++;

    TransformComponent transform;
	transform.position = position;
    transform.rotation = rotation;
	transformComponents[entity] = transform;

	PhysicsComponent physics;
	physics.velocity = velocity;
	physicsComponents[entity] = physics;

    return entity;
}

Mesh App::make_cube_mesh(glm::vec3 size) {

    float l = size.x/2;
    float w = size.y/2;
    float h = size.z/2;

    std::vector<float> vertices = {
         l,  w, -h, 1.0f, 1.0f,
         l, -w, -h, 1.0f, 0.0f,
        -l, -w, -h, 0.0f, 0.0f,
        -l, -w, -h, 0.0f, 0.0f,
        -l,  w, -h, 0.0f, 1.0f,
         l,  w, -h, 1.0f, 1.0f,

        -l, -w,  h, 0.0f, 0.0f,
         l, -w,  h, 1.0f, 0.0f,
         l,  w,  h, 1.0f, 1.0f,
         l,  w,  h, 1.0f, 1.0f,
        -l,  w,  h, 0.0f, 1.0f,
        -l, -w,  h, 0.0f, 0.0f,

        -l,  w,  h, 1.0f, 1.0f,
        -l,  w, -h, 1.0f, 0.0f,
        -l, -w, -h, 0.0f, 0.0f,
        -l, -w, -h, 0.0f, 0.0f,
        -l, -w,  h, 0.0f, 1.0f,
        -l,  w,  h, 1.0f, 1.0f,

         l, -w, -h, 0.0f, 0.0f,
         l,  w, -h, 1.0f, 0.0f,
         l,  w,  h, 1.0f, 1.0f,
         l,  w,  h, 1.0f, 1.0f,
         l, -w,  h, 0.0f, 1.0f,
         l, -w, -h, 0.0f, 0.0f,

        -l, -w, -h, 0.0f, 0.0f,
         l, -w, -h, 1.0f, 0.0f,
         l, -w,  h, 1.0f, 1.0f,  
         l, -w,  h, 1.0f, 1.0f,
        -l, -w,  h, 0.0f, 1.0f,
        -l, -w, -h, 0.0f, 0.0f,

         l,  w,  h, 1.0f, 1.0f,
         l,  w, -h, 1.0f, 0.0f,
        -l,  w, -h, 0.0f, 0.0f, 
        -l,  w, -h, 0.0f, 0.0f,
        -l,  w,  h, 0.0f, 1.0f,
         l,  w,  h, 1.0f, 1.0f
    };

    std::vector<unsigned int> indices = {
        0, 1, 2, 3, 4, 5,
        6, 7, 8, 9, 10, 11,
        12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23,
        24, 25, 26, 27, 28, 29,
        30, 31, 32, 33, 34, 35
    };

    MeshData data;
    data.indices = indices;
    data.vertices = vertices;

    Mesh mesh = meshSystem->createMesh(data);
    return mesh;
}

double lastTime = glfwGetTime();
double lastDT = glfwGetTime();
void App::run() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        
        double currentTime = glfwGetTime();
        float dt = float(currentTime - lastDT);
        lastDT = currentTime;

        motionSystem->update(transformComponents, physicsComponents, dt);
        cameraSystem->update(transformComponents, physicsComponents, cameraID, *cameraComponent, dt);

		renderSystem->update(transformComponents, renderComponents);

        App::fpsCounter++;
        if (currentTime - lastTime >= 1.0) {
            App::fps = App::fpsCounter;
            App::fpsCounter = 0;
            lastTime += 1.0;
        }
	}
}

void App::set_up_glfw() {

    glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
	
	window = glfwCreateWindow(800, 480, "Mescraft", NULL, NULL);
	glfwMakeContextCurrent(window);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSwapInterval(0);

    glfwSetFramebufferSizeCallback(window, window_size_changed_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cout << "Couldn't load opengl" << std::endl;
		glfwTerminate();
	}

}

void App::window_size_changed_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);

    GLint currentShader = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentShader);

    unsigned int projLocation = glGetUniformLocation(currentShader, "projection");
    float aspect = static_cast<float>(width) / static_cast<float>(height);

	glm::mat4 projection = glm::perspective(App::fov, aspect, 0.01f, 1000.0f);
	glUniformMatrix4fv(projLocation, 1, GL_FALSE, glm::value_ptr(projection));
}

void App::set_up_opengl() {
    glClearColor(0.25f, 0.5f, 0.75f, 1.0f);

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // --- Load shaders ---
    shader3D = make_shader("./src/shaders/vertex.txt", "./src/shaders/fragment.txt");
    shader2D = make_shader("./src/shaders/vertex2D.txt", "./src/shaders/fragment2D.txt");
    shaderText = make_shader("./src/shaders/vertex_text.txt", "./src/shaders/fragment_text.txt");

    // --- Setup 3D shader ---
    glUseProgram(shader3D);
    int proj3DLoc = glGetUniformLocation(shader3D, "projection");
    int texLoc = glGetUniformLocation(shader3D, "textureSampler");
    glUniform1i(texLoc, 0); // texture unit 0

    float aspect = static_cast<float>(w) / static_cast<float>(h);
    glm::mat4 proj3D = glm::perspective(fov, aspect, 0.01f, 1000.0f);
    glUniformMatrix4fv(proj3DLoc, 1, GL_FALSE, glm::value_ptr(proj3D));

    // --- Setup Text shader ---
    glUseProgram(shaderText);
    int projTextLoc = glGetUniformLocation(shaderText, "projection");
    int modelTextLoc = glGetUniformLocation(shaderText, "model");
    int colorTextLoc = glGetUniformLocation(shaderText, "textColor");

    glm::mat4 projText = glm::ortho(0.0f, float(w), float(h), 0.0f); // screen-space ortho
    glm::mat4 modelText = glm::mat4(1.0f); // identity for now

    glUniformMatrix4fv(projTextLoc, 1, GL_FALSE, glm::value_ptr(projText));
    glUniformMatrix4fv(modelTextLoc, 1, GL_FALSE, glm::value_ptr(modelText));
    glUniform3f(colorTextLoc, 1.0f, 1.0f, 1.0f); // default text color (white)

    // --- Setup 2D shader ---
    glUseProgram(shader2D);
    int proj2DLoc = glGetUniformLocation(shader2D, "projection");
    int color2DLoc = glGetUniformLocation(shader2D, "color");       // optional if shader uses a color
    int tex2DLoc   = glGetUniformLocation(shader2D, "texture0");    // your texture sampler
    int useTextureLoc = glGetUniformLocation(shader2D, "useTexture");

    glm::mat4 proj2D = glm::ortho(0.0f, float(w), float(h), 0.0f); // screen-space ortho
    glUniformMatrix4fv(proj2DLoc, 1, GL_FALSE, glm::value_ptr(proj2D));
    glUniform3f(color2DLoc, 1.0f, 1.0f, 1.0f);                     // default white
    glUniform1i(tex2DLoc, 0); 
    glUniform1i(useTextureLoc, 0);  // no texture, just color
}


void App::make_systems() {
    world = new World(0);

    motionSystem = new MotionSystem();
    cameraSystem = new CameraSystem(shader3D, window);
    renderSystem = new RenderSystem(shader3D, shaderText, shader2D, window, world, transformComponents);
    meshSystem = new MeshSystem();

    
}