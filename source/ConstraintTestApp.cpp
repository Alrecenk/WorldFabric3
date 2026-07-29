#include "ConstraintTestApp.h"
#include "ParticlePlugin.h"
#include "ScenePlugin.h"
#include "FlagSet.h"


void ConstraintTestApp::Ball::integrateVelocity(float dt) {
	position += velocity * dt;
}

void ConstraintTestApp::Ball::integrateAcceleration(float dt) {
	velocity += acceleration * dt;
}

ConstraintTestApp::Ball::~Ball() {
	getTool<ScenePlugin>()->deleteInstance(instance_id);
}

std::shared_ptr<ConstraintTestApp::Ball> ConstraintTestApp::PhysicsCell::getBall(int64_t id) {
	auto iter = balls.find(id);
	if (iter != balls.end()) {
		return iter->second;
	}
	else {
		return nullptr;
	}
}

//Consraint id should be a hash of the involved bodies and the type of constraint
std::shared_ptr<ConstraintTestApp::Constraint> ConstraintTestApp::PhysicsCell::getConstraint(int64_t id) {
	auto iter = constraints.find(id);
	if (iter != constraints.end()) {
		return iter->second;
	}
	else {
		return nullptr;
	}
}



// Called when switching into this state before the first time run is called
void ConstraintTestApp::enter(std::shared_ptr<MachineState> from) {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	//Load the pawn model
	scene->createModelSet(Ball::baLL_model, Ball::baLL_model, true);

	// Make a particle for the mouse
	mouse_particle_id = particles->createParticle(0);
	particles->setColor(mouse_particle_id, glm::vec4(1, 0, 0, 1));
	glm::mat4 particle_pose = glm::mat4(1.0f);
	particles->setPose(mouse_particle_id, particle_pose);

	// Set up a light for the scene
	ScenePlugin::LightComponent lc;
	glm::vec3 light_position = glm::vec3(15, 15, -5);
	glm::vec3 look_at = glm::vec3(0, 0, 0);
	lc.light_color = glm::vec4(1, 1, 1, 1);
	light_id = scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_position, look_at, glm::vec3(0, 1, 0), 0.55f, 30, 1024, 0, lc);

	// Place the camera
	glm::vec3 camera_position = { 0,20,-30 };
	float fov = 1.0f;
	window->window_target->setCamera(camera_position, look_at, fov, glm::vec3(0, 1, 0));


	glm::vec3 min = {-5,-5,-5} ;
	glm::vec3 max = {5,5,5};
	cell = std::shared_ptr<PhysicsCell>(new PhysicsCell(min,max)) ;


	float start_speed = 1.0f ;
	float gravity = 1.0f ;
	for(int k=0;k<20;k++){
		glm::vec3 pos = {(randomFloat()-0.5f)*8.0f,(randomFloat() - 0.5f) * 8.0f,(randomFloat() - 0.5f) * 8.0f} ;
		glm::vec3 vel = { (randomFloat() - 0.5f) * start_speed,(randomFloat() - 0.5f) * start_speed,(randomFloat() - 0.5f) * start_speed };
		glm::vec3 acc = {0,-gravity,0} ;
		cell->addBall(pos,vel,acc);
	}
}

//Called every frame while the state is active
void ConstraintTestApp::run() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	// Get the current time and time slice of the frame
	current_time = now();
	float dt = microsBetween(last_run_time, current_time) / 1000000.0f;
	last_run_time = current_time;

	// get the 3D ray from the mouse position on the screen
	glm::vec3 ray_origin = window->window_target->camera_position;
	glm::vec3 ray_direction = window->getMouseRay();

	float t = -1 ; // TODO implement raytracing to make balls clickable
	//Place the mouse particle
	glm::vec3 mouse_position;
	if (t > 0) { // collision
		particles->setColor(mouse_particle_id, glm::vec4(0, 0, 1, 1)); // blue
		mouse_position = window->window_target->camera_position + window->getMouseRay() * t; // hit postion
	}
	else { // no collision
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