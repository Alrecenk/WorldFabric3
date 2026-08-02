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
	pose = glm::scale(pose, glm::vec3(radius,radius,radius));
	pose = pose * glm::mat4_cast(orientation) ;
	if(instance_id == -1){
		instance_id = scene->createInstance(BALL_MODEL,pose);
	}else{
		scene->setPose(instance_id, pose);
	}
}

void ConstraintTestApp::Ball::integrateVelocity(float dt) {
	position += velocity * dt;

	// Update orientation quaternion
	// dq/dt = 0.5 * omega * q
	glm::quat omega_quat(0, angular_velocity.x, angular_velocity.y, angular_velocity.z);
	orientation += (omega_quat * orientation) * (0.5f * dt);
	orientation = glm::normalize(orientation);
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


void ConstraintTestApp::BallCollision::updateConstraintTarget(PhysicsCell* cell){
	auto ball_1 = cell->getBall(id1);
	auto ball_2 = cell->getBall(id2);

	float velocity_against_normal = glm::dot(ball_1->velocity - ball_2->velocity, normal);

	float restitution_bias = 0.0f; // inelastic
	if (velocity_against_normal > min_velocity_for_elastic) {
		float e = (ball_1->elasticity + ball_2->elasticity) * 0.5f;
		restitution_bias = e * velocity_against_normal; // elastic
	}

	//Bias against penetration with spring force
	float penetration = (ball_1->radius + ball_2->radius) - glm::distance(ball_1->position, ball_2->position);
	float penetration_bias = 0 ; // penetration_spring_coefficient * std::max(0.0f, penetration - allowed_collision_depth);

	target = restitution_bias + penetration_bias;
}
void ConstraintTestApp::BallCollision::applyWarmingImpulse(PhysicsCell* cell){
	auto b1 = cell->getBall(id1);
	auto b2 = cell->getBall(id2);

	b1->velocity -= warm_impulse * b1->inv_mass;
	b2->velocity += warm_impulse * b2->inv_mass;
}
void ConstraintTestApp::BallCollision::applyConstraint(PhysicsCell* cell) {
	//TODO AI code, needs cleanup and simplication but is verified to work !
	auto b1 = cell->getBall(id1);
	auto b2 = cell->getBall(id2);

	// 1. Lever arms (Center of mass -> Contact point)
	glm::vec3 r1 = point - b1->position;
	glm::vec3 r2 = point - b2->position;

	// 2. Calculate Velocity at the exact point of contact
	// Vp = V_linear + (AngularVel x r)
	glm::vec3 v1_at_p = b1->velocity + glm::cross(b1->angular_velocity, r1);
	glm::vec3 v2_at_p = b2->velocity + glm::cross(b2->angular_velocity, r2);
	glm::vec3 rel_vel = v2_at_p - v1_at_p;

	float vel_along_normal = glm::dot(rel_vel, normal);

	// 3. Calculate Effective Mass (K)
	// For spheres, the inverse inertia tensor is just a scalar.
	// For polyhedrons, you'd do: glm::vec3 rot_term = glm::cross(invInertiaWorld * glm::cross(r, normal), r);
	float rot_term1 = glm::dot(glm::cross(b1->inv_inertia * glm::cross(r1, normal), r1), normal);
	float rot_term2 = glm::dot(glm::cross(b2->inv_inertia * glm::cross(r2, normal), r2), normal);

	float effective_mass = b1->inv_mass + b2->inv_mass + rot_term1 + rot_term2;
	if (effective_mass == 0.0f) return;

	// 4. Normal Impulse (Same clamping logic as before)
	float impulse_mag_n = -(vel_along_normal - target) / effective_mass;
	float old_accumulated_n = glm::dot(warm_impulse, normal);
	float new_accumulated_n = std::max(0.0f, old_accumulated_n + impulse_mag_n);
	float actual_impulse_n = new_accumulated_n - old_accumulated_n;

	glm::vec3 impulse_vec_n = normal * actual_impulse_n;

	// --- APPLY NORMAL IMPULSE ---
	// Linear
	b1->velocity -= impulse_vec_n * b1->inv_mass;
	b2->velocity += impulse_vec_n * b2->inv_mass;
	// Angular: Delta Omega = I^-1 * (r x Impulse)
	b1->angular_velocity -= b1->inv_inertia * glm::cross(r1, impulse_vec_n);
	b2->angular_velocity += b2->inv_inertia * glm::cross(r2, impulse_vec_n);

	warm_impulse += impulse_vec_n;

	// --- PART 2: FRICTION (TANGENT) ---
	// Recalculate velocities at point after normal impulse
	v1_at_p = b1->velocity + glm::cross(b1->angular_velocity, r1);
	v2_at_p = b2->velocity + glm::cross(b2->angular_velocity, r2);
	rel_vel = v2_at_p - v1_at_p;

	glm::vec3 tangent = rel_vel - (glm::dot(rel_vel, normal) * normal);
	float tangent_len = glm::length(tangent);

	if (tangent_len > 0.0001f) {
		tangent = glm::normalize(tangent);

		// Effective mass for tangent direction
		float rot_term1_t = glm::dot(glm::cross(b1->inv_inertia * glm::cross(r1, tangent), r1), tangent);
		float rot_term2_t = glm::dot(glm::cross(b2->inv_inertia * glm::cross(r2, tangent), r2), tangent);
		float effective_mass_t = b1->inv_mass + b2->inv_mass + rot_term1_t + rot_term2_t;

		float vel_along_tangent = glm::dot(rel_vel, tangent);
		float impulse_mag_t = -vel_along_tangent / effective_mass_t;

		float max_friction = friction_coefficient * new_accumulated_n;
		float old_accumulated_t = glm::dot(warm_tangent_impulse, tangent);
		float new_accumulated_t = std::min(std::max(old_accumulated_t + impulse_mag_t, -max_friction), max_friction);
		float actual_impulse_t = new_accumulated_t - old_accumulated_t;

		glm::vec3 impulse_vec_t = tangent * actual_impulse_t;

		// Apply Tangent Impulse
		b1->velocity -= impulse_vec_t * b1->inv_mass;
		b2->velocity += impulse_vec_t * b2->inv_mass;
		b1->angular_velocity -= b1->inv_inertia * glm::cross(r1, impulse_vec_t);
		b2->angular_velocity += b2->inv_inertia * glm::cross(r2, impulse_vec_t);

		warm_tangent_impulse += impulse_vec_t;
	}
}

void ConstraintTestApp::BallWallCollision::updateConstraintTarget(PhysicsCell* cell) {
	auto b = cell->getBall(id);

	float velocity_against_normal = -1.0f * glm::dot(b->velocity, normal);

	float e = b->elasticity;
	float restitution_bias = 0.0f; // inelastic
	if (velocity_against_normal > min_velocity_for_elastic) {
		restitution_bias = e * velocity_against_normal; // elastic
	}

	//Resolve penetration with spring force
	float dist = glm::dot(b->position - point, normal);
	float penetration = b->radius - dist;
	float penetration_bias =  penetration_spring_coefficient * std::max(0.0f, penetration - allowed_collision_depth);

	target = restitution_bias + penetration_bias;
}
void ConstraintTestApp::BallWallCollision::applyWarmingImpulse(PhysicsCell* cell) {
	auto b = cell->getBall(id);
	b->velocity += warm_impulse * b->inv_mass;
}
void ConstraintTestApp::BallWallCollision::applyConstraint(PhysicsCell* cell) {
	auto b = cell->getBall(id);

	glm::vec3 r = point - b->position;//lever arm for torque
	glm::vec3 contact_velocity = b->velocity + glm::cross(b->angular_velocity, r);
	float velocity_along_normal = glm::dot(contact_velocity, normal);
	float effective_mass_n = b->inv_mass + glm::dot(glm::cross(b->inv_inertia * glm::cross(r, normal), r), normal);

	if (effective_mass_n == 0.0f){
		return; 
	}

	float impulse_magnitude = (target - velocity_along_normal) / effective_mass_n;

	//Adjust considering existing warm impulse
	float old_accumulated_n = glm::dot(warm_impulse, normal);
	float new_accumulated_n = std::max(0.0f, old_accumulated_n + impulse_magnitude); // can only push
	float current_impulse_n = new_accumulated_n - old_accumulated_n;

	//apply impulse along normal
	glm::vec3 impulse_vec_n = normal * current_impulse_n;
	b->velocity += impulse_vec_n * b->inv_mass;
	b->angular_velocity += b->inv_inertia * glm::cross(r, impulse_vec_n);
	//update warm impulse
	warm_impulse += impulse_vec_n;

	// Recalculate velocity at point after normal impulse is applied
	contact_velocity = b->velocity + glm::cross(b->angular_velocity, r);

	// Find the tangent vector
	glm::vec3 tangent = contact_velocity - (glm::dot(contact_velocity, normal) * normal);
	float tangent_len2 = glm::length2(tangent);

	if (tangent_len2 > 0.000001f) {
		tangent = glm::normalize(tangent);
		
		// Effective mass for the tangent direction
		float effective_mass_t = b->inv_mass + glm::dot(glm::cross(b->inv_inertia * glm::cross(r, tangent), r), tangent);

		float velocity_along_tangent = glm::dot(contact_velocity, tangent);
		float impulse_mag_t =  -1.0f *  velocity_along_tangent / effective_mass_t;

		// Clamps to friction coeffciient and consider arm tangent impulse
		float max_friction = friction_coefficient * new_accumulated_n;
		float old_accumulated_t = glm::dot(warm_tangent_impulse, tangent);
		float new_accumulated_t = std::min(std::max(old_accumulated_t + impulse_mag_t, -max_friction), max_friction);
		float current_impulse_t = new_accumulated_t - old_accumulated_t;

		//Apply tangent impulse
		glm::vec3 impulse_vec_t = tangent * current_impulse_t;
		b->velocity += impulse_vec_t * b->inv_mass;
		b->angular_velocity += b->inv_inertia * glm::cross(r, impulse_vec_t);

		//update warm tangent impulse
		warm_tangent_impulse += impulse_vec_t;
	}


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
		constraint->updateConstraintTarget(this);
	}
	for (auto& [id, constraint] : constraints) {
		constraint->applyWarmingImpulse(this);
	}
	for(int i = 0 ; i < constraints_iter; i++){
		for (auto& [id, constraint] : constraints) {
			constraint->applyConstraint(this);
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


	cell = std::make_shared<PhysicsCell>(min,max) ;


	/*
	float start_speed = 0.02f ;
	float gravity = 2.0f ;
	for(int k=0;k<20;k++){
		glm::vec3 pos = {min.x + (0.2f + randomFloat()*0.6f) * (max.x-min.x),min.y + (0.2f + randomFloat() * 0.6f) * (max.y - min.y),min.z + (0.2f + randomFloat() * 0.6f) * (max.z - min.z) } ;
		glm::vec3 vel = { (randomFloat() - 0.5f) * start_speed,(randomFloat() - 0.5f) * start_speed,(randomFloat() - 0.5f) * start_speed };
		glm::vec3 acc = {0,-gravity,0} ;
		cell->addBall(pos,vel,acc);
	}
*/
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


	if(millisBetween(last_ball_time,current_time) > millis_between_balls && cell->balls.size() < max_balls){
		last_ball_time = current_time ;
		glm::vec3 pos = { min.x + (0.4f + randomFloat() * 0.2f) * (max.x - min.x),max.y-1.0f,min.z + 0.5f };
		glm::vec3 vel = { (randomFloat() - 0.5f) * 1.0f,(randomFloat() - 0.5f) * 1.0f,randomFloat() * 5.0f};
		glm::vec3 acc = { 0,-gravity,0 };
		auto id = cell->addBall(pos, vel, acc);
		cell->balls[id]->angular_velocity = glm::vec3(3,0,0);
	}


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