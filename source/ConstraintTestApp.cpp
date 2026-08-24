#include "ConstraintTestApp.h"
#include "ParticlePlugin.h"
#include "ScenePlugin.h"
#include "FlagSet.h"


ConstraintTestApp::PhysicsCell::PhysicsCell(){


}

//Custom destructor cleans up scene instance
ConstraintTestApp::PhysicsCell::~PhysicsCell(){
	ScenePlugin* scene = getTool<ScenePlugin>();
	for(auto& [ id, type_sceneid] : instance){
		scene->deleteInstance(type_sceneid.second) ;
	}
}

int ConstraintTestApp::PhysicsCell::addType(std::shared_ptr<Physics::ConvexShape> shape, const std::string& model, float render_scale){
	int id = next_type_id ;
	next_type_id++;
	types[id] = {shape, model, render_scale} ;
	return id ;
}

int64_t ConstraintTestApp::PhysicsCell::add(int type, const glm::vec3& pos, const glm::vec3& vel, const glm::vec3& a_vel){
	int64_t id = next_object_id++;
	ScenePlugin* scene = getTool<ScenePlugin>();
	instance[id] = {type, scene->createInstance(types[type].model, glm::mat4(0))};
	bodies[id] = std::make_shared<Physics::RigidBody>(types[type].shape, id, pos, vel, a_vel);
	return id ;
}

Physics::RigidBody* ConstraintTestApp::PhysicsCell::getBody(int64_t id) {
	auto iter = bodies.find(id);
	if (iter != bodies.end()) {
		return iter->second.get();
	}
	else {
		return nullptr;
	}
}

//Consraint id should be a hash of the involved bodies and the type of constraint
Physics::Constraint* ConstraintTestApp::PhysicsCell::getConstraint(int64_t id) {
	auto iter = constraints.find(id);
	if (iter != constraints.end()) {
		return iter->second.get();
	}
	else {
		return nullptr;
	}
}

//Finds all collisions of the balls with each other and the walls of the cell
//Creates or destroys constraints so the contents of constraints matches the current collisions
//Also sets points and normal for collisions
void ConstraintTestApp::PhysicsCell::updateCollisions(){
	std::unordered_set<int64_t> found_constraints ;
	for(auto& [id1,body_1] : bodies){
		//Ball to ball collisions
		for (auto& [id2, body_2] : bodies) {
			if(id1 < id2 && // only check each pair once
				(body_1->shape->inv_mass > 0 || body_2->shape->inv_mass > 0)  && // only check if one is moveable
				Physics::AAABIntersect(body_1->AABB, body_2->AABB)){ // check AABBs first
				auto simplex = Physics::detectCollision(body_1.get(), body_2.get()) ;
				if(simplex.size() > 0){
					Physics::SupportPoint sp = Physics::getPenetration(simplex, body_1.get(), body_2.get()) ;
					if(glm::length(sp.x) > Physics::Collision::allowed_collision_depth * 0.5f){
						glm::vec3 point = (sp.a+sp.b)*0.5f;
						glm::vec3 normal = glm::normalize(sp.x) ;

						normal = glm::normalize(normal) ;
						int64_t constraint_id = getConstraintID(id1,id2,Physics::Collision::CONSTRAINT_TYPE) ;
						found_constraints.insert(constraint_id); // track found so we can remove not found
						auto iter = constraints.find(constraint_id) ;
						std::shared_ptr<Physics::Collision> constraint ;
						//Add constraint if it doesn't exist already
						if(iter == constraints.end()){
							constraint = std::make_shared<Physics::Collision>();
							constraints[constraint_id] = constraint ;
						}else{
							constraint = static_pointer_cast<Physics::Collision>(iter->second) ;
						}
						//update constraint data
						constraint->id1= id1 ;
						constraint->id2 = id2 ;
						constraint->point = point;
						constraint->normal = normal;
						constraint->penetration = sp ;
					}
				}
			}
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
	for(auto& [id,body] : bodies){
		body->integrateAcceleration(acceleration, dt);
	}
	for (auto& [id, constraint] : constraints) {
		constraint->updateConstraint(this);
	}
	for (auto& [id, constraint] : constraints) {
		constraint->applyWarmingImpulse(this);
	}
	for(int i = 0 ; i < constraints_iter; i++){
		for (auto& [id, constraint] : constraints) {
			constraint->applyConstraint(this);
		}
	}
	for (auto& [id, ball] : bodies) {
		ball->integrateVelocity(dt);
	}
	updateCollisions();
}

//Calls update graphics on all the balls
//Also renders the box
void ConstraintTestApp::PhysicsCell::updateGraphics(){
	ScenePlugin* scene = getTool<ScenePlugin>();
	for(auto& [id,ball] : bodies){
		if(id > 0){ // it's a ball
			auto iter = instance.find(id);
			glm::mat4 pose = glm::mat4(1.0f);
			pose = glm::translate(pose, ball->position);
			float scale= types[iter->second.first].render_scale ;
			pose = glm::scale(pose, glm::vec3(scale,scale,scale));
			pose = pose * glm::mat4_cast(ball->orientation);
			scene->setPose(instance[id].second, pose);
		}
	}
}

ConstraintTestApp::ConstraintTestApp() {}

// Called when switching into this state before the first time run is called
void ConstraintTestApp::enter(std::shared_ptr<MachineState> from) {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	
	

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


	cell = std::make_shared<PhysicsCell>() ;

	float ball_radius = 0.5f ;
	std::shared_ptr<Physics::Sphere> ball_shape = std::make_shared<Physics::Sphere>(ball_radius, 1.0f);
	scene->createModelSet(BALL_MODEL, BALL_MODEL, true);
	ball_type = cell->addType(ball_shape, BALL_MODEL,ball_radius) ;

	
	float box_size = 0.9f ;
	std::shared_ptr<Physics::ConvexPolyhedron> box_shape = std::make_shared< Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeAxisAlignedBox(glm::vec3(box_size, box_size, box_size)));
	box_shape->inv_mass = ball_shape->inv_mass;
	box_shape->inv_moment = ball_shape->inv_moment; // TODo shouldbe compuited correctly autmatically
	std::shared_ptr<GLTF> box = std::make_shared<GLTF>();
	box->setBoundingBoxModel(glm::vec3(-box_size * 0.5, -box_size * 0.5f, -box_size * 0.5f), glm::vec3(box_size * 0.5f, box_size * 0.5f, box_size * 0.5f), glm::vec4(0.5, 0.5, 1, 1));
	scene->createModelSet("box", box, false, false);
	box_type = cell->addType(box_shape, "box", 1.0f);

	float wall_size = 10.0f ;
	std::shared_ptr<GLTF> wall = std::make_shared<GLTF>();
	wall->setBoundingBoxModel(glm::vec3(-wall_size * 0.5, -wall_size * 0.5f, -wall_size * 0.5f), glm::vec3(wall_size * 0.5f, wall_size * 0.5f, wall_size * 0.5f), glm::vec4(1, 1, 1, 1));
	scene->createModelSet("wall", wall, false, false);
	std::shared_ptr<Physics::ConvexPolyhedron> wall_shape = std::make_shared< Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeAxisAlignedBox(glm::vec3(wall_size, wall_size, wall_size)));
	wall_shape->inv_mass = 0; // infinite mass makes walls immovable
	wall_shape->inv_moment = glm::mat3(0);
	wall_type = cell->addType(wall_shape, "wall", 1.0f);

	glm::vec3 mid = (min + max) * 0.5f;
	cell->add(wall_type, glm::vec3(mid.x, min.y - wall_size * 0.5f, mid.z)) ;
	cell->add(wall_type, glm::vec3(max.x + wall_size * 0.5f, mid.y, mid.z));
	cell->add(wall_type, glm::vec3(min.x - wall_size * 0.5f, mid.y, mid.z));
	cell->add(wall_type, glm::vec3(mid.x, mid.y, min.z - wall_size * 0.5f));
	cell->add(wall_type, glm::vec3(mid.x, mid.y, max.z + wall_size * 0.5f));

}

//Called every frame while the state is active
void ConstraintTestApp::run() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	// Get the current time and time slice of the frame
	current_time = now();
	float dt = microsBetween(last_run_time, current_time) / 1000000.0f;
	
	if(dt <= 0.001f || dt > 0.5f){ 
		dt = 0.001f ; // don't move on frames where something is amiss with the clock
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
	cell->runPhysicsFrame(dt, 20);
	cell->updateGraphics();


	if(millisBetween(last_ball_time,current_time) > millis_between_balls && cell->bodies.size() < max_balls){
		last_ball_time = current_time ;
		glm::vec3 pos = { min.x + (0.4f + randomFloat() * 0.2f) * (max.x - min.x),max.y-1.0f,min.z + 0.5f };
		glm::vec3 vel = { (randomFloat() - 0.5f) * 1.0f,(randomFloat() - 0.5f) * 1.0f,1.0f+randomFloat() * 4.0f};
		int type = randomFloat()<0.2f ? box_type : ball_type ;
		auto id = cell->add(type, pos, vel, glm::vec3(randomFloat()*2.0f-1.0f, randomFloat() * 2.0f-1.0f, randomFloat() * 2.0f-1.0f));
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