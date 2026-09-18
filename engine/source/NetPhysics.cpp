#include "NetPhysics.h"
#include "ScenePlugin.h"
#include "VolumeNode.h"
#include "BSPNode.h"
#include <stack>

namespace NetPhysics{


	RigidBody::RigidBody(local_ptr<ShapeSet>& s, int64_t i, const glm::vec3& p, const glm::vec3& v, const glm::vec3& w){
		shape = s ;
		id = i;
		position = p;
		velocity = v;
		angular_velocity = w;

		float mass = 0;
		glm::mat3 moment(0) ;
		for(auto& part : shape){
			mass+= part.mass ;
			moment += part.moment ;
		}
		if(mass <=0){ // immobile objects have 0 mass and inv_mass
			base_inv_moment = glm::mat3(0);
			inv_mass = 0 ;
		}else{
			base_inv_moment = glm::inverse(moment);
			inv_mass = 1.0f/ mass ;
		}

	}

	//Create a rigid body from the static object type list on the RigidBodyView
	RigidBody::RigidBody(int view_type, const glm::vec3& p, const glm::vec3& v, const glm::vec3& av){
		render_type = view_type ;
		shape = RigidBodyView::types[render_type].shape ;
		position = p ;
		velocity = v, 
		angular_velocity = av ;
		elasticity = RigidBodyView::types[render_type].elasticity ;
		friction = RigidBodyView::types[render_type].friction;

		float mass = 0;
		glm::mat3 moment(0);
		for (auto& part : shape) {
			mass += part.mass;
			moment += part.moment;
		}
		if (mass <= 0) { // immobile objects have 0 mass and inv_mass
			base_inv_moment = glm::mat3(0);
			inv_mass = 0;
		}
		else {
			base_inv_moment = glm::inverse(moment);
			inv_mass = 1.0f / mass;
		}

	}

void RigidBody::integrateVelocity(float dt){
	position += velocity * dt;
	// Update orientation quaternion
	// dq/dt = 0.5 * omega * q
	glm::quat omega_quat(0, angular_velocity.x, angular_velocity.y, angular_velocity.z);
	orientation += (omega_quat * orientation) * (0.5f * dt);
	orientation = glm::normalize(orientation);

	pose = glm::mat4(1.0f);
	pose = glm::translate(pose, position);
	pose = pose * glm::mat4_cast(orientation);
	inv_pose = glm::inverse(pose);

	glm::mat3 r = glm::mat3_cast(orientation);
	inv_moment = r * base_inv_moment * glm::transpose(r);
	AABB = { {FLT_MAX,FLT_MAX,FLT_MAX},{-FLT_MAX,-FLT_MAX,-FLT_MAX} };
	for(auto& s : shape){
		auto  sAABB = s.getAABB(pose);
		AABB.first.x = fmin(AABB.first.x, sAABB.first.x) ;
		AABB.second.x = fmax(AABB.second.x, sAABB.second.x);
		AABB.first.y = fmin(AABB.first.y, sAABB.first.y);
		AABB.second.y = fmax(AABB.second.y, sAABB.second.y);
		AABB.first.z = fmin(AABB.first.z, sAABB.first.z);
		AABB.second.z = fmax(AABB.second.z, sAABB.second.z);
	}
}

void RigidBody::integrateAcceleration(float dt){
	if (inv_mass <= 0) { // don't accelerate objects with infinite mass
		return;
	}
	velocity += acceleration * dt;

	
	float speed = glm::length(velocity);
	if(speed < drag*dt){
		velocity = glm::vec3(0,0,0) ;
	}else{
		velocity *= (speed-drag*dt)/speed ;
	}

	float angular_speed = glm::length(angular_velocity);
	if (angular_speed < angular_drag*dt) {
		angular_velocity = glm::vec3(0, 0, 0);
	}
	else {
		angular_velocity *= (angular_speed - angular_drag*dt) / angular_speed;
	}
	
}


//Walks through state machine to run each physics step in lockstep with other elements
void RigidBody::runPhysics(){

	if(inv_mass == 0){ // don't run movement on events on immoveable objects
		return ;
	}

	double frame_length = 1.0 / PhysicsCell::ticks_per_second ;
	double slice_time = frame_length / PhysicsCell::frame_slices;
	int frame = (int)(time * PhysicsCell::ticks_per_second + slice_time * 0.25) ; // offset makes sure rounding error doesn't cause round down into wrong frame
	double frame_time = time - frame*frame_length;
	int frame_step = (int)(frame_time / slice_time + 0.25) ;


	

	//Steps:
	// 0 = integrate acceleraton
	// 1 = update constraint targets
	// 2 = apply warming
	// for 0 <=k < constraint_iterations 
	// 3 + 2k = apply constraint
	//3 + 2k + 1 = collect impulses
	//3 + 2 * constrant_iterations = integrate velocity
	//3 + 2 * constrant_iterations + 1 to frame time  = update collisionsand find constraints


	//printf(" %lld Run Physics time: %lf frame: %d, frame_length: %lf, step: %d  :", id, time, frame, frame_time, frame_step) ;
	if(frame_step == 0 ){
		integrateAcceleration((float)frame_length) ;
		queue(id, frame * frame_length + slice_time * 2, &RigidBody::runPhysics) ;
		//printf("acceleration %lf \n", frame * frame_length + slice_time * 2);
	}else if(frame_step == 2){
		//TODO apply warming
		queue(id, frame * frame_length + slice_time * 4, &RigidBody::runPhysics);
		//printf("warming %lf \n", frame * frame_length + slice_time * 4);
	}else if(frame_step > 3 && frame_step < 3 + 2 * PhysicsCell::constraint_iterations && frame_step%2 == 0){
		//TODO collect impulses
		int next_step =std::min(frame_step+2, 3 + 2 * PhysicsCell::constraint_iterations) ;
		queue(id, frame*frame_length + slice_time * next_step, &RigidBody::runPhysics);
		//printf("collect \n");
	}else if(frame_step == 3 + 2 * PhysicsCell::constraint_iterations){
		integrateVelocity((float)frame_length) ;
		queue(id, (frame+1) * frame_length, &RigidBody::runPhysics);
		//printf(" %lf -> velocity -> %lf\n",time, (frame + 1) * frame_length);
	}else{ // We're off step, wait until next frame and try again
		queue(id, (frame + 1) * frame_length, &RigidBody::runPhysics);
		//printf("%lf -> out of sync %d -> %lf\n",time,frame_step, (frame + 1) * frame_length);
	}
}

//created is called when an objectis observed that ws no observed last time view was called on the world
void RigidBodyView::created(std::shared_ptr<const RigidBody>& body){
	last_view = body;
	glm::mat4 pose = glm::mat4(1.0f);
	pose = glm::translate(pose, body->position);
	pose = pose * glm::mat4_cast(body->orientation);
	pose = pose * types[body->render_type].render_transform;
	ScenePlugin* scene = getTool<ScenePlugin>();
	scene_id = scene->createInstance(types[body->render_type].model, pose);
}

//Update is called when an observation is made of an object that was also observed last frame on this same view
void RigidBodyView::updated(std::shared_ptr<const RigidBody>& body){
	last_view = body;
	glm::mat4 pose = glm::mat4(1.0f);
	pose = glm::translate(pose, body->position);
	pose = pose * glm::mat4_cast(body->orientation);
	pose = pose * types[body->render_type].render_transform;
	ScenePlugin* scene = getTool<ScenePlugin>();
	scene->setPose(scene_id, pose);
}

//Destroyed is called when an observation that was present in the last observation is no longer observed
//This view will be deleted immediately after this call (it's destructor will be called after this)
void RigidBodyView::destroyed(){
	ScenePlugin* scene = getTool<ScenePlugin>();
	scene->deleteInstance(scene_id);
}


int RigidBodyView::addType(std::shared_ptr<Physics::ConvexShape> shape, const std::string& model, glm::mat4& render_transform, float elasticity, float friction){
	int id = next_type_id;
	next_type_id++;
	types[id] = { ShapeSet(shape), model, render_transform, elasticity, friction };
	return id;
}


int RigidBodyView::addType(std::vector<std::shared_ptr<Physics::ConvexShape>> shape, const std::string& model, glm::mat4& render_transform, float elasticity, float friction){
	int id = next_type_id;
	next_type_id++;
	types[id] = { ShapeSet(shape), model, render_transform, elasticity, friction };
	return id;
}


int RigidBodyView::addType(std::vector<Physics::ConvexPolyhedron> raw_shape, const std::string& model, glm::mat4& render_transform, float elasticity, float friction){
	std::vector<std::shared_ptr<Physics::ConvexShape>> shape;
	for (auto& s : raw_shape) {
		std::shared_ptr<Physics::ConvexPolyhedron> sh = std::make_shared<Physics::ConvexPolyhedron>(s, render_transform, s.mass);
		shape.push_back(sh);
	}
	return addType(shape, model, render_transform, elasticity, friction);
}



int64_t Collision::getHash() const {
	return getHash(id1, shape1, id2, shape2);
}
void Collision::updateConstraint(WorldObject* owner) {
	std::shared_ptr<const RigidBody> body_1 = owner->read<RigidBody>(id1);
	std::shared_ptr<const RigidBody> body_2 = owner->read<RigidBody>(id2);

	//lever arms for torque
	glm::vec3 r1 = point - body_1->position;
	glm::vec3 r2 = point - body_2->position;
	glm::vec3 contact_velocity_1 = body_1->velocity + glm::cross(body_1->angular_velocity, r1);
	glm::vec3 contact_velocity_2 = body_2->velocity + glm::cross(body_2->angular_velocity, r2);
	float velocity_against_normal = glm::dot(contact_velocity_1 - contact_velocity_2, normal);

	float restitution_bias = 0.0f; // inelastic
	if (velocity_against_normal > min_velocity_for_elastic) {
		float e = fmax(body_1->elasticity, body_2->elasticity);
		restitution_bias = e * velocity_against_normal; // elastic
	}

	//Bias against penetration with spring force
	float penetration_bias = penetration_spring_coefficient * std::max(0.0f, penetration_depth - allowed_collision_depth);

	target = restitution_bias + penetration_bias;
}
void Collision::setWarmingImpulse(WorldObject* owner) {
	std::shared_ptr<const RigidBody> body_1 = owner->read<RigidBody>(id1);
	std::shared_ptr<const RigidBody> body_2 = owner->read<RigidBody>(id2);

	warm_tangent_impulse -= normal * glm::dot(normal, warm_tangent_impulse);
	warm_impulse = normal * glm::dot(normal, warm_impulse);

	tangents.clear();
	glm::vec3 ref = (std::abs(normal.y) < 0.8f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
	tangents.push_back(glm::normalize(glm::cross(normal, ref)));
	tangents.push_back(glm::normalize(glm::cross(normal, tangents[0])));

	next_impulse = warm_impulse + warm_tangent_impulse;

}
void Collision::setConstraintImpulse(WorldObject* owner) {
	std::shared_ptr<const RigidBody> body_1 = owner->read<RigidBody>(id1) ;
	std::shared_ptr<const RigidBody> body_2 = owner->read<RigidBody>(id2) ;

	//lever arms for torque
	glm::vec3 r1 = point - body_1->position;
	glm::vec3 r2 = point - body_2->position;

	glm::vec3 contact_velocity_1 = body_1->velocity + glm::cross(body_1->angular_velocity, r1);
	glm::vec3 contact_velocity_2 = body_2->velocity + glm::cross(body_2->angular_velocity, r2);
	glm::vec3 relative_velocity = contact_velocity_2 - contact_velocity_1;
	float velocity_along_normal = glm::dot(relative_velocity, normal);

	//calculate effective mass
	float rot_term1 = glm::dot(glm::cross(body_1->inv_moment * glm::cross(r1, normal), r1), normal);
	float rot_term2 = glm::dot(glm::cross(body_2->inv_moment * glm::cross(r2, normal), r2), normal);
	float effective_mass = body_1->inv_mass + body_2->inv_mass + rot_term1 + rot_term2;
	if (effective_mass <= 1e-6f) {
		return; // two immovable objects
	}

	//Calculate current change needed based on already applied
	float impulse_mag_n = (target - velocity_along_normal) / effective_mass;
	float old_accumulated = glm::dot(warm_impulse, normal);
	float new_accumulated = std::max(0.0f, old_accumulated + impulse_mag_n);
	float current_impulse = new_accumulated - old_accumulated;
	glm::vec3 impulse = normal * current_impulse;

	//update warm impulse
	warm_impulse += impulse;

	// Recalculate velocities at point after normal impulse
	contact_velocity_1 = body_1->velocity + glm::cross(body_1->angular_velocity, r1);
	contact_velocity_2 = body_2->velocity + glm::cross(body_2->angular_velocity, r2);
	relative_velocity = contact_velocity_2 - contact_velocity_1;


	glm::vec3 tangent_impulse(0);
	for (auto& tangent : tangents) {
		float velocity_along_tangent = glm::dot(tangent, relative_velocity);

		// Effective mass for tangent direction
		float rot_term1_t = glm::dot(glm::cross(body_1->inv_moment * glm::cross(r1, tangent), r1), tangent);
		float rot_term2_t = glm::dot(glm::cross(body_2->inv_moment * glm::cross(r2, tangent), r2), tangent);
		float effective_mass_t = body_1->inv_mass + body_2->inv_mass + rot_term1_t + rot_term2_t;
		if (effective_mass_t < 1e-6f) {
			continue;
		}
		//Compute maximum tangent velocity ot be lost
		float impulse_mag_t = -1.0f * velocity_along_tangent / effective_mass_t;
		tangent_impulse += tangent * impulse_mag_t; ;
	}

	glm::vec3 accumulated_friction = tangent_impulse + warm_tangent_impulse;
	float friction_magnitude = glm::length(accumulated_friction);
	if (friction_magnitude < 1e-6f) {
		accumulated_friction = glm::vec3(0, 0, 0);
	}
	else {
		float max_friction = (body_1->friction + body_2->friction) * 0.5f * new_accumulated;
		float clamped_magnitude = std::min(friction_magnitude, max_friction);
		accumulated_friction *= clamped_magnitude / friction_magnitude;
	}
	tangent_impulse = accumulated_friction - warm_tangent_impulse;

	//update warm impulse
	warm_tangent_impulse = accumulated_friction;


	float scale = 1.0f/ std::max(body_1->constraints.size(), body_1->constraints.size()) ;
	next_impulse = scale * ( impulse + tangent_impulse) ;

}

//Retargets this constraint to the objects after it has moved
bool Collision::retargetConstraint(WorldObject* owner) {
	std::shared_ptr<const RigidBody> body_1 = owner->read<RigidBody>(id1);
	std::shared_ptr<const RigidBody> body_2 = owner->read<RigidBody>(id2);
	glm::vec3 a = body_1->pose * glm::vec4(local_a, 1);
	glm::vec3 b = body_2->pose * glm::vec4(local_b, 1);
	glm::vec3 x = a - b;
	float new_depth = glm::length(x);
	glm::vec3 new_normal = x / new_depth;
	if (glm::dot(normal, new_normal) < retarget_normal_alignment_minimum) {
		return false;
	}
	point = (a + b) * 0.5f;
	penetration_depth = new_depth;
	return true;
}

//Returns an identifying hash that can be used to group constraints into this set
int64_t ManifoldCollision::getHash() const {
	return hash;
}

//Add a constraint to this set
void ManifoldCollision::addConstraint(const Collision& new_point) {
	std::vector<int> to_keep;
	int closest = -1;
	float cd2 = FLT_MAX;

	for (int k = 0; k < points.size(); k++) {
		bool valid = points[k].retargetConstraint(this);
		if (valid) {
			to_keep.push_back(k);
			if (glm::distance2(points[k].point, new_point.point) < cd2) {
				closest = k;
			}
		}
	}

	//Point is so close it's the same point
	if (closest >= 0 && cd2 < squared_distance_for_match) {
		points[closest].local_a = new_point.local_a;
		points[closest].local_b = new_point.local_b; // overwrite with new point 
		points[closest].point = new_point.point;
		points[closest].normal = new_point.normal;
		// but carry over warm impulses
	}
	else if (closest >= 0 && points.size() >= max_collision_points) { // Too many collision points
		points[closest] = new_point; // overwrite with new point
		// dont carry over warm impulses
	}
	else { //We can have a totally new point
		to_keep.push_back((int)points.size());
		points.push_back(new_point);
	}

	std::vector<Collision> new_points;
	for (int k : to_keep) {
		new_points.push_back(points[k]);
	}
	points = new_points;

}

//Update the constraint targets based on information at the start of the frame
//Returns if any of the constraints are active at all
void ManifoldCollision::updateConstraints() {
	for (auto& p : points) {
		p.updateConstraint(this);
	}
}

//Apply starting impulses carried over if any constraint has existed for multiple frames in a row
void ManifoldCollision::setWarmingImpulses() {
	for (auto& p : points) {
		p.setWarmingImpulse(this);
	}
}

//Applies impulses to velocity of involved bodies to satisfy these constraints
void ManifoldCollision::setConstraintImpulses() {
	for (auto& p : points) {
		p.setConstraintImpulse(this);
	}
}

//Walks through state machine to run each physics step in lockstep with other elements
void ManifoldCollision::runPhysics() {
	//TODO
}


void PhysicsCell::addBody(const int64_t& new_body){
	bodies.push_back(new_body) ;
	queue(new_body,time,&RigidBody::runPhysics) ;
}


//Walks through state machine to run each physics step in lockstep with other elements
void PhysicsCell::runPhysics() {
	//TODO
}

void registerPhysics(){
	WorldPlugin* worlds = getTool<WorldPlugin>();
	worlds->registerClass<RigidBody, RigidBodyView>("RigidBody");
	worlds->registerMethod(&RigidBody::runPhysics, "RigidBody::runPhysics");
	
	worlds->registerClass<PhysicsCell>("Cell");
	worlds->registerMethod(&PhysicsCell::addBody,"addBody") ;
	worlds->registerMethod(&PhysicsCell::runPhysics, "PhysicsCell::runPhysics");

	worlds->registerClass<ManifoldCollision>("Collision Set");
	worlds->registerMethod(&ManifoldCollision::addConstraint, "add collision constraint");
	worlds->registerMethod(&ManifoldCollision::runPhysics, "Collision::runPhysics");
}




} // end namespace Physics