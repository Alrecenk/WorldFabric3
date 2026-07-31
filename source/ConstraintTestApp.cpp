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
	getTool<ScenePlugin>()->deleteInstance(instance_id);
}

int64_t ConstraintTestApp::PhysicsCell::addBall(const glm::vec3& pos, const glm::vec3& vel, const glm::vec3& acc){
	int64_t id = next_ball_id++;
	balls[id] = std::make_shared<Ball>(id, pos,vel,acc);
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

void ConstraintTestApp::PhysicsCell::updateWallCollision(int64_t ball_id, int wall_id, const glm::vec3& point, const glm::vec3& normal, std::unordered_set<int64_t>& found_constraints){
	int64_t constraint_id = getConstraintID(ball_id, wall_id, BallWallCollision::CONSTRAINT_TYPE);
	found_constraints.insert(constraint_id); // track found so we can remove not found
	auto iter = constraints.find(constraint_id);
	std::shared_ptr< BallWallCollision> constraint;
	//Add constraint if it doesn't exist already
	if (iter == constraints.end()) {
		constraint = std::make_shared<BallWallCollision>();
		constraints[constraint_id] = constraint;
	}
	else {
		constraint = static_pointer_cast<BallWallCollision>(iter->second);
	}
	//update constraint data
	constraint->id = ball_id;
	constraint->point = point;
	constraint->normal = normal;

}

//Finds all collisions of the balls with each other and the walls of the cell
//Creates or destroys constraints so the contents of constraints matches the current collisions
//Also sets points and normal for collisions
void ConstraintTestApp::PhysicsCell::updateCollisions(){
	std::unordered_set<int64_t> found_constraints ;
	for(auto& [id1,ball_1] : balls){
		//Ball to ball collisions
		for (auto& [id2, ball_2] : balls) {
			if(id1 < id2){ // only check each once
				float d2 = glm::distance2(ball_1->position, ball_2->position) ;
				float rl = (ball_1->radius + ball_2->radius) ;
				if(d2 < rl*rl){ // squared distance check avoids sqrt
					glm::vec3 point = (ball_1->radius * ball_2->position + ball_2->radius * ball_1->position) /rl;
					glm::vec3 normal = ball_2->position - ball_1->position;
					normal = glm::normalize(normal) ;
					int64_t constraint_id = getConstraintID(id1,id2,BallCollision::CONSTRAINT_TYPE) ;
					found_constraints.insert(constraint_id); // track found so we can remove not found
					auto iter = constraints.find(constraint_id) ;
					std::shared_ptr< BallCollision> constraint ;
					//Add constraint if it doesn't exist already
					if(iter == constraints.end()){
						constraint = std::make_shared<BallCollision>();
						constraints[constraint_id] = constraint ;
					}else{
						constraint = static_pointer_cast<BallCollision>(iter->second) ;
					}
					//update constraint data
					constraint->id1= id1 ;
					constraint->id2 = id2 ;
					constraint->point = point;
					constraint->normal = normal;
				}
			}
		}

		//Ball to wall collisons
		if(ball_1->position.x + ball_1->radius > max.x){
			glm::vec3 point = ball_1->position ;
			point.x = max.x ;
			glm::vec3 normal(-1, 0 , 0) ;
			updateWallCollision(id1,1,point,normal,found_constraints) ;
		}
		if (ball_1->position.x - ball_1->radius < min.x) {
			glm::vec3 point = ball_1->position;
			point.x = min.x;
			glm::vec3 normal(1, 0, 0);
			updateWallCollision(id1, 2, point, normal, found_constraints);
		}
		if (ball_1->position.y + ball_1->radius > max.y) {
			glm::vec3 point = ball_1->position;
			point.y = max.y;
			glm::vec3 normal(0, -1, 0);
			updateWallCollision(id1, 3, point, normal, found_constraints);
		}
		if (ball_1->position.y - ball_1->radius < min.y) {
			glm::vec3 point = ball_1->position;
			point.y = min.y;
			glm::vec3 normal(0, 1, 0);
			updateWallCollision(id1, 4, point, normal, found_constraints);
		}
		if (ball_1->position.z + ball_1->radius > max.z) {
			glm::vec3 point = ball_1->position;
			point.z = max.z;
			glm::vec3 normal(0, 0, -1);
			updateWallCollision(id1, 5, point, normal, found_constraints);
		}
		if (ball_1->position.z - ball_1->radius < min.z) {
			glm::vec3 point = ball_1->position;
			point.z = min.z;
			glm::vec3 normal(0, 0, 1);
			updateWallCollision(id1, 6, point, normal, found_constraints);
		}
			
	}

	//Delete existing constraints not found now
	std::vector<int64_t> to_delete ;
	for(auto& [id, constraint] : constraints){
		if(found_constraints.find(id) == found_constraints.end()){
			to_delete.push_back(id) ;
		}
	}
	for(auto& id : to_delete){
		constraints.erase(id) ;
	}
}

//Run physics forward one frame
void ConstraintTestApp::PhysicsCell::runPhysicsFrame(float dt, int constraints_iter){
	for(auto& [id,ball] : balls){
		ball->integrateAcceleration(dt);
	}
	for (auto& [id, constraint] : constraints) {
		constraint->updateConstraintTarget();
	}
	for (auto& [id, constraint] : constraints) {
		constraint->applyWarmingImpulse();
	}
	for(int i = 0 ; i < constraints_iter; i++){
		for (auto& [id, constraint] : constraints) {
			constraint->applyConstraint();
		}
	}
	for (auto& [id, ball] : balls) {
		ball->integrateVelocity(dt);
	}
	updateCollisions();
}

//Calls update graphics on all the balls
//Also renders the box
void ConstraintTestApp::PhysicsCell::updateGraphics(){
	for(auto& [id,ball] : balls){
		ball->updateGraphics();
	}
	if(instance_id == -1){
		ScenePlugin* scene = getTool<ScenePlugin>();
		std::shared_ptr<GLTF> box = std::make_shared<GLTF>() ;
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
	cell = std::make_shared<PhysicsCell>(min,max) ;


	float start_speed = 1.0f ;
	float gravity = 0.3f ;
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
	if(dt < 0 || dt > 0.5f){ 
		dt = 0 ; // don't move on frames where something is amiss with the clock
	}
	
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