#include "ConstraintTestApp.h"
#include "ParticlePlugin.h"
#include "ScenePlugin.h"
#include "FlagSet.h"


ConstraintTestApp::Ball::Ball(int64_t my_id, const glm::vec3& pos, const glm::vec3& vel, const glm::vec3& acc){
	id = my_id ;
	position = pos ;
	velocity = vel ;
	acceleration = acc ;
}


void ConstraintTestApp::Ball::updateGraphics(){
	ScenePlugin* scene = getTool<ScenePlugin>();
	glm::mat4 pose = glm::mat4(1.0f);
	pose = glm::translate(pose,position) ;
	if(instance_id == -1){
		instance_id = scene->createInstance(BALL_MODEL,pose);
	}else{
		scene->setPose(instance_id, pose);
	}
}

void ConstraintTestApp::Ball::integrateVelocity(float dt) {
	position += velocity * dt;
}

void ConstraintTestApp::Ball::integrateAcceleration(float dt) {
	velocity += acceleration * dt;
}

ConstraintTestApp::Ball::~Ball() {
	getTool<ScenePlugin>()->deleteInstance(instance_id);
}

ConstraintTestApp::PhysicsCell::PhysicsCell(const glm::vec3& box_min, const glm::vec3& box_max){
	min = box_min ;
	max = box_max ;
}

//Custom destructor cleans up scene instance
ConstraintTestApp::PhysicsCell::~PhysicsCell(){

}

int64_t ConstraintTestApp::PhysicsCell::addBall(const glm::vec3& pos, const glm::vec3& vel, const glm::vec3& acc){
	int64_t id = next_ball_id++;
	balls[id] = std::shared_ptr<Ball>(new Ball(id, pos,vel,acc));
	return id ;
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


void ConstraintTestApp::BallCollision::updateConstraintTarget(){
	//TODO
}
void ConstraintTestApp::BallCollision::applyWarmingImpulse(){
	//TODO
}
void ConstraintTestApp::BallCollision::applyConstraint(){
	//TODO
}

void ConstraintTestApp::BallWallCollision::updateConstraintTarget() {
	//TODO
}
void ConstraintTestApp::BallWallCollision::applyWarmingImpulse() {
	//TODO
}
void ConstraintTestApp::BallWallCollision::applyConstraint() {
	//TODO
}


//Finds all collisions of the balls with each other and the walls of the cell
		//Creates or destroys constraints so the contents of constraints matches the current collisions
void ConstraintTestApp::PhysicsCell::updateCollisions(){
	//TODO
}

//Run physics forward one frame
void ConstraintTestApp::PhysicsCell::runPhysicsFrame(float dt, int constraints_iter){
	//TODO

}

//Calls update graphics on all the balls
//Also renders the box
void ConstraintTestApp::PhysicsCell::updateGraphics(){
	for(auto& [id,ball] : balls){
		ball->updateGraphics();
	}
	//TODO render container
	if(instance_id == -1){
		ScenePlugin* scene = getTool<ScenePlugin>();
		std::shared_ptr<GLTF> box = std::shared_ptr<GLTF>(new GLTF()) ;
		box->setBoundingBoxModel(min,max, glm::vec4(1,1,1,1));
		box = box->createMirrorImage() ; // Flips winding order inside out
		scene->createModelSet(BOX_MODEL,box) ;
		instance_id = scene->createInstance(BOX_MODEL,glm::mat4(1.0f) );
	}
}

ConstraintTestApp::ConstraintTestApp() {}

// Called when switching into this state before the first time run is called
void ConstraintTestApp::enter(std::shared_ptr<MachineState> from) {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	//Load the pawn model
	scene->createModelSet(Ball::BALL_MODEL, Ball::BALL_MODEL, true);

	// Make a particle for the mouse
	mouse_particle_id = particles->createParticle(0);
	particles->setColor(mouse_particle_id, glm::vec4(1, 0, 0, 1));
	glm::mat4 particle_pose = glm::mat4(1.0f);
	particles->setPose(mouse_particle_id, particle_pose);

	// Set up a light for the scene
	ScenePlugin::LightComponent lc;
	glm::vec3 light_position = glm::vec3(15, 15, -5);
	glm::vec3 look_at = glm::vec3(0, 0, 0);
	lc.light_color = glm::vec4(0.5, 0.5, 0.5, 1);
	light_id = scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_position, look_at, glm::vec3(0, 1, 0), 0.55f, 30, 2048, 0, lc);

	// Place the camera
	glm::vec3 camera_position = { 0,20,-3 };
	float fov = 0.7f;
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


	updateCamera();

	cell->runPhysicsFrame(dt, 10);
	cell->updateGraphics();

	// Check if escape pressed to exit
	if (window->getLastKeyPress() == SDLK_ESCAPE) {
		getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
	}
}

// Called when switching out of this state after the last time run is called
void ConstraintTestApp::exit(std::shared_ptr<MachineState> to) {
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	particles->destroyParticle(mouse_particle_id);
}

void ConstraintTestApp::updateCamera() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	if (window->mouseDown(3)) { // right mouse button
		if (!mouse_down_right) {
			mouse_down_position_right = window->getMousePosition();
			camera_down_thi = camera_thi;
			camera_down_theta = camera_theta;
		}
		glm::vec2 mouse_position = window->getMousePosition();
		mouse_down_right = true;
		camera_theta = camera_down_theta + camera_x_speed * (mouse_position.x - mouse_down_position_right.x);
		camera_thi = camera_down_thi + camera_y_speed * (mouse_position.y - mouse_down_position_right.y);
		camera_thi = fmax(fmin(camera_thi, 3.14159f * 0.5f), 0.0f);
		mouse_down_position_right = window->getMousePosition();
		camera_down_thi = camera_thi;
		camera_down_theta = camera_theta;

	}
	else {
		mouse_down_right = false;
	}

	if (mouse_wheel_y_previous < window->getMouseWheelPosition().y) {
		zoom *= 0.95f;
	}
	else if (mouse_wheel_y_previous > window->getMouseWheelPosition().y) {
		zoom /= 0.95f;
	}

	if (zoom < 1.0f) {
		zoom = 1.0f;
	}
	mouse_wheel_y_previous = window->getMouseWheelPosition().y;

	glm::vec3 camera_position = glm::vec3(cosf(camera_theta) * cosf(camera_thi), sinf(camera_thi), sinf(camera_theta) * cosf(camera_thi)) * zoom;
	window->window_target->setCamera(camera_position, look_at, fov, glm::vec3(0, 1, 0));

	glm::vec3 light_position = glm::vec3(cosf(light_theta) * cosf(light_thi), sinf(light_thi), sinf(light_theta) * cosf(light_thi)) * light_zoom;

	scene->moveLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_id, light_position, light_look_at, glm::vec3(0, 1, 0), light_fov, 30);
}