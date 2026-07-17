#include "TraceApp.h"
#include "ParticlePlugin.h"
#include "ScenePlugin.h"
#include "FlagSet.h"

TraceApp::TraceApp() {}

// Called when switching into this state before the first time run is called
void TraceApp::enter(std::shared_ptr<MachineState> from) {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	//Load the pawn model
	scene->createModelSet("pawn", "./assets/pawn.glb", true);

	//Make an instance of the pawm
	glm::mat4 pawn_pose = glm::mat4(1.0f);
	pawn_id = scene->createInstance("pawn", pawn_pose);
	
	// Make a particle for the mouse
	mouse_particle_id = particles->createParticle(0);
	particles->setColor(mouse_particle_id, glm::vec4(1, 0, 0, 1));
	glm::mat4 particle_pose = glm::mat4(1.0f);
	particles->setPose(mouse_particle_id, particle_pose);

	// Set up a light for the scene
	ScenePlugin::LightComponent lc;
	glm::vec3 light_position = glm::vec3(2, 2, -5);
	glm::vec3 look_at = glm::vec3(0, 0, 0);
	lc.light_color = glm::vec4(1, 1, 1, 1);
	light_effect_id = scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_position, look_at, glm::vec3(0, 1, 0), 0.55f, 30, 1024, 0, lc);

	// Place the camera
	glm::vec3 camera_position = { 0,2,-3 };
	float fov = 1.0f;
	window->window_target->setCamera(camera_position, look_at, fov, glm::vec3(0, 1, 0));
}

//Called every frame while the state is active
void TraceApp::run() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	// Get the current time and time slice of the frame
	current_time = now();
	float dt = microsBetween(last_run_time, current_time) / 1000000.0f;
	last_run_time = current_time;

	//Rotate the pawn around an arbitrary axis
	current_angle += dt;
	glm::vec3 axis = glm::vec3(1, 2, 0);
	glm::mat4 pawn_pose = glm::mat4(1.0f);
	pawn_pose = glm::rotate(pawn_pose, current_angle, axis);
	scene->setPose(pawn_id, pawn_pose);

	// get the 3D ray from the mouse position on the screen
	glm::vec3 ray_origin = window->window_target->camera_position;
	glm::vec3 ray_direction = window->getMouseRay();

	// Move the mouse ray into the model's coordinates
	glm::mat4 scene_to_model_space = glm::inverse(pawn_pose);
	ray_origin = scene_to_model_space * glm::vec4(ray_origin, 1); // positions have 1 in slot 4 to include translation
	ray_direction = scene_to_model_space * glm::vec4(ray_direction, 0); // directions have 0 in slot 4 to not include translation
	
	//Trace the model
	std::shared_ptr<GLTF> model = scene->getModelController("pawn");
	float t = model->rayTrace(ray_origin, ray_direction);
	
	//Place the mouse particle
	glm::vec3 mouse_position;
	if (t > 0) { // collision
		particles->setColor(mouse_particle_id, glm::vec4(0, 0, 1, 1)); // blue
		mouse_position = window->window_target->camera_position + window->getMouseRay() * t; // hit postion
	} else { // no collision
		particles->setColor(mouse_particle_id, glm::vec4(1, 0, 0, 1)); // red
		mouse_position = window->window_target->camera_position + window->getMouseRay() * 3.0f; // arbitrary depth on no collision
	}
	glm::mat4 particle_pose = glm::mat4(1.0f);
	particle_pose = glm::translate(particle_pose, mouse_position);
	particle_pose = glm::scale(particle_pose, glm::vec3(0.03, 0.03, 0.03));
	particles->setPose(mouse_particle_id, particle_pose);

	// Check if escape pressed to exit
	if (window->getLastKeyPress() == SDLK_ESCAPE) {
		getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
	}
}

// Called when switching out of this state after the last time run is called
void TraceApp::exit(std::shared_ptr<MachineState> to) {
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	scene->deleteInstance(pawn_id);
	particles->destroyParticle(mouse_particle_id);
}