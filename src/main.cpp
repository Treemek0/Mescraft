#include "controller/app.h"

#include "components/camera_component.h"
#include "components/physics_component.h"
#include "components/render_component.h"
#include "components/transform_component.h"
#include "entities/player.h"

static App* app;

int main() {

	std::cout << __cplusplus << std::endl;

	app = new App();

	app->set_up_opengl();

	unsigned int cameraEntity = app->make_entity({0.0f, 72.0f, 1.0f}, {0.0f, 90.0f, 0.0f}, {0.0f, 0.0f, 0.0f});

	CameraComponent* camera = new CameraComponent();
	app->cameraComponent = camera;
	app->cameraID = cameraEntity;

	app->make_systems();

	// unsigned int cubeEntity = app->make_entity({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
	
	// RenderComponent cubeRender;
	// cubeRender.mesh = app->make_cube_mesh({1.0f, 1.0f, 1.0f});
	// cubeRender.material = app->renderSystem->make_texture("textures/mazik.png");
	// app->renderComponents.emplace(cubeEntity, std::move(cubeRender));

	app->run();

	delete app;
	return 0;
}