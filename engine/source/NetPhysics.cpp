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

		integrateVelocity(0);
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

		integrateVelocity(0);
	}

void RigidBody::integrateVelocity(float dt){
	position += velocity * dt;
	//printf("moved: %f,%f,%f\n", velocity.x * dt, velocity.y * dt, velocity.z * dt) ;
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

void RigidBody::applyConstraintImpulses(){
	std::vector<int64_t> new_constraints ;
	for(const int64_t& c_id : constraints){
		std::shared_ptr<const ManifoldCollision> manifold= read<ManifoldCollision>(c_id);
		if(manifold){
			for(const auto& c : manifold->points){
				glm::vec3 r = c.point - position;
				if(c.id1 == id){
					velocity -= c.next_impulse * inv_mass;
					angular_velocity -= inv_moment * glm::cross(r, c.next_impulse);
				}else{
					velocity += c.next_impulse * inv_mass;
					angular_velocity += inv_moment * glm::cross(r, c.next_impulse);
				}
				//printf("  Applying impulse: %f, %f, %f\n", c.next_impulse.x, c.next_impulse.y, c.next_impulse.z) ;
			}
			new_constraints.push_back(c_id) ; // only keep constraints we could read
		}
	}
	constraints = new_constraints ;
}

void RigidBody::addConstraints(const std::vector<int64_t>& new_constraints){
	for(auto& constraint_id : new_constraints){
		constraints.push_back(constraint_id) ;
	}
}


//Walks through state machine to run each physics step in lockstep with other elements
void RigidBody::runPhysics(){

	if(inv_mass == 0){ // don't run movement on events on immoveable objects
		//printf("%lld run disabled because it is immoveable\n", id);
		return ;
	}

	double frame_length = 1.0 / Cell::ticks_per_second ;
	double slice_time = frame_length / Cell::frame_slices;
	int frame = (int)(time * Cell::ticks_per_second + slice_time * 0.25) ; // offset makes sure rounding error doesn't cause round down into wrong frame
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
		//printf("%lld at %lf step %d: Integrate acceleration \n", id, time, frame_step);
	}else if(frame_step == 2){
		//printf("%lld at %lf step %d: Applying warming impulses \n", id, time, frame_step);
		applyConstraintImpulses();
		queue(id, frame * frame_length + slice_time * 4, &RigidBody::runPhysics);
	}else if(frame_step > 3 && frame_step < 3 + 2 * Cell::constraint_iterations && frame_step%2 == 0){
		//printf("%lld at %lf step %d :Apply constraint impulses \n", id, time, frame_step);
		applyConstraintImpulses();
		int next_step =std::min(frame_step+2, 3 + 2 * Cell::constraint_iterations) ;
		queue(id, frame*frame_length + slice_time * next_step, &RigidBody::runPhysics);
	}else if(frame_step == 3 + 2 * Cell::constraint_iterations){
		integrateVelocity((float)frame_length) ;
		queue(id, (frame+1) * frame_length, &RigidBody::runPhysics);
		//printf("%lld at %lf step %d: Integrate Velocity\n",id, time, frame_step);
	}else{ // We're off step, wait until next frame and try again
		queue(id, (frame + 1) * frame_length, &RigidBody::runPhysics);
		//printf("%lld (RigidBody) at %lf step(%d) : Out of Sync delaying to next frame time(%lf)\n",id, time,frame_step, (frame + 1) * frame_length);
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
	if(!body_1){
		printf("Couldn't find constraint body!: %lld\n", id1);
		return ;
	}
	if (!body_2) {
		printf("Couldn't find constraint body!: %lld\n", id2);
		return ;
	}
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

	//Set the warming impulse
	warm_tangent_impulse -= normal * glm::dot(normal, warm_tangent_impulse);
	warm_impulse = normal * glm::dot(normal, warm_impulse);

	tangents.clear();
	glm::vec3 ref = (std::abs(normal.y) < 0.8f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
	tangents.push_back(glm::normalize(glm::cross(normal, ref)));
	tangents.push_back(glm::normalize(glm::cross(normal, tangents[0])));

	
	next_impulse = warm_impulse + warm_tangent_impulse;

	//printf("warm impulse: %f, %f, %f\n", next_impulse.x, next_impulse.y, next_impulse.z) ;

}
void Collision::setConstraintImpulse(WorldObject* owner) {
	std::shared_ptr<const RigidBody> body_1 = owner->read<RigidBody>(id1) ;
	std::shared_ptr<const RigidBody> body_2 = owner->read<RigidBody>(id2) ;

	float scale = 1.0f / std::max(body_1->constraints.size(), body_2->constraints.size());

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
	warm_impulse += scale*impulse;

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
	warm_tangent_impulse += scale* tangent_impulse;


	
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
	bool needs_start = points.size() == 0 ;
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
	//printf("Manifold size: %d\n", (int)points.size());

	if(needs_start){
		double frame_length = 1.0 / Cell::ticks_per_second;
		double slice_time = frame_length / Cell::frame_slices;
		int frame = (int)(time * Cell::ticks_per_second + slice_time * 0.25); // offset makes sure rounding error doesn't cause round down into wrong frame
		double frame_time = time - frame * frame_length;
		int frame_step = (int)(frame_time / slice_time + 0.25);
		//printf("Constraint created on frame : %d\n", frame) ;
		queue(id, (frame + 1) * frame_length + slice_time * 1, &ManifoldCollision::runPhysics);
	}

	last_update_time = time ;
	position = new_point.point ;

}

//Update the constraint targets based on information at the start of the frame
//Returns if any of the constraints are active at all
void ManifoldCollision::updateConstraints() {
	for (auto& p : points) {
		p.updateConstraint(this);
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

	double frame_length = 1.0 / Cell::ticks_per_second;
	double slice_time = frame_length / Cell::frame_slices;
	int frame = (int)(time * Cell::ticks_per_second + slice_time * 0.25); // offset makes sure rounding error doesn't cause round down into wrong frame
	double frame_time = time - frame * frame_length;
	int frame_step = (int)(frame_time / slice_time + 0.25);

	//Ifwre didn't get a constrairt update within the last frame, then not coliding anymore
	if(time - last_update_time > frame_length){
		destroyed = true ;
		//printf("Constraint timed out without access! %lf, %lf frame: %d\n", time, last_update_time, frame);
		return ;
	}


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
	if (frame_step == 1) {
		//printf("%lld at %lf step %d : Update Consraint target and Warming\n", id, time, frame_step);
		updateConstraints() ;
		queue(id, frame * frame_length + slice_time * 3, &ManifoldCollision::runPhysics);
	}else if (frame_step >= 3 && frame_step < 3 + 2 * Cell::constraint_iterations && frame_step % 2 == 1) {
		//printf("%lld at %lf step %d : Calculate Constraint Impulse\n", id, time, frame_step);
		setConstraintImpulses() ;
		int next_step = frame_step + 2 ;
		if(next_step < 3 + 2 * Cell::constraint_iterations){
			queue(id, frame * frame_length + slice_time * next_step, &ManifoldCollision::runPhysics);
		}else{ // goto next frame
			queue(id, (frame+1) * frame_length + slice_time * 1, &ManifoldCollision::runPhysics);
		}
	}else { // We're off step, wait until next frame and try again
		queue(id, (frame + 1) * frame_length + slice_time * 1, &ManifoldCollision::runPhysics);
		//printf("%lld (Collision Manifold) at %lf step %d : out of sync moving to next frame time(%lf)\n", id, time, frame_step, (frame + 1) * frame_length + slice_time * 1);
	}
}


void Cell::addBody(const int64_t& new_body){
	bodies.push_back(new_body) ;
	queue(new_body,time,&RigidBody::runPhysics) ;
}


void Cell::updateCollisions() {

	double frame_length = 1.0 / Cell::ticks_per_second;
	double slice_time = frame_length / Cell::frame_slices;
	int frame = (int)(time * Cell::ticks_per_second + slice_time * 0.25); // offset makes sure rounding error doesn't cause round down into wrong frame


	std::unordered_map<int64_t,std::shared_ptr<const RigidBody>> read_bodies ;
	for(int64_t& id : bodies){
		std::shared_ptr<const RigidBody> body = read<RigidBody>(id) ;
		if(body){
			read_bodies[id] = body ;
			//printf("read body: %lld\n", id);
		}else{
			//printf("read failed for body: %lld\n", id) ;
			
		}
	}

	//Delete existing constraints not found now
	std::vector<int64_t> to_delete;
	for (auto& [hash, id] : constraints) {
		std::shared_ptr<const ManifoldCollision> existing = read<ManifoldCollision>(id);
		if (!existing) {
			//printf("Can't find existing constraint %lld frame: %d\n", id, frame) ;
			to_delete.push_back(hash);
		}
	}
	for (auto& id : to_delete) {
		constraints.erase(id);
	}

	std::unordered_map<int64_t, std::vector<int64_t>> new_body_collisions ;

	for (auto& [id1, body_1] : read_bodies) {
		for (auto& [id2, body_2] : read_bodies) {
			if (id1 < id2 && // only check each pair once
				(body_1->inv_mass > 0 || body_2->inv_mass > 0) && // only check if one is moveable
				Physics::AAABIntersect(body_1->AABB, body_2->AABB)) { // check AABBs first
				//printf("AABBs are colliding\n");
				int index_a = 0 ;
				int index_b = 0 ;
				for(const auto& shape_a : body_1->shape){
					for (const auto& shape_b : body_2->shape) {
						
						auto simplex = detectCollision(body_1.get(), &shape_a, body_2.get(), &shape_b);
						if (simplex.size() > 0) {
							//printf("Collision detected!\n");
							Physics::SupportPoint sp = Physics::getPenetration(simplex, body_1.get(), &shape_a, body_2.get(), &shape_b);
							//printf("Penetration length: %f\n", glm::length(sp.x)) ;
							if (glm::length(sp.x) > Collision::allowed_collision_depth * 0.5f) {
								//printf("Penetration exceeds allowed depth\n");
								glm::vec3 point = (sp.a + sp.b) * 0.5f;
								glm::vec3 normal = glm::normalize(sp.x);

								normal = glm::normalize(normal);
								int64_t constraint_hash = Collision::getHash(id1, index_a, id2, index_b);

								Collision constraint ;
								constraint.id1 = id1;
								constraint.shape1 = index_a;
								constraint.id2 = id2;
								constraint.shape2 = index_b;
								constraint.point = point;
								constraint.normal = normal;
								constraint.local_a = body_1->inv_pose * glm::vec4(sp.a, 1);
								constraint.local_b = body_2->inv_pose * glm::vec4(sp.b, 1);
								constraint.penetration_depth = glm::length(sp.x);

								//printf("collision point %lld, %lld: %f, %f, %f\n", id1, id2, point.x, point.y,point.z) ;
								
								if (constraints.find(constraint_hash) == constraints.end()) {
									std::shared_ptr<ManifoldCollision> new_set = std::make_shared<ManifoldCollision>(constraint_hash) ;
									new_set->position = point ;
									constraints[constraint_hash] = create(new_set,time);
									//printf("Contraint requested on frame: %d\n", frame) ;
									//queue(id1,time,&RigidBody::addConstraint, constraints[constraint_hash]) ;
									//queue(id2, time, &RigidBody::addConstraint, constraints[constraint_hash]);
									if(body_1->inv_mass > 0){
										new_body_collisions[id1].push_back(constraints[constraint_hash]) ;
									}
									if (body_2->inv_mass > 0) {
										new_body_collisions[id2].push_back(constraints[constraint_hash]);
									}
								}else{
									//printf("Found existing constraint on frame: %d\n", frame);
								}
								queue(constraints[constraint_hash],time+time+1E-7,&ManifoldCollision::addConstraint, constraint) ;
								
							}
						}
						index_b++;
					}
					index_a++;
				}
				
			}
			
		}

	}


	for(auto& [ body_id, new_constraints] :new_body_collisions){
		queue(body_id, time, &RigidBody::addConstraints, new_constraints);
	}
	
	

}

//Walks through state machine to run each physics step in lockstep with other elements
void Cell::runPhysics() {

	double frame_length = 1.0 / Cell::ticks_per_second;
	double slice_time = frame_length / Cell::frame_slices;
	int frame = (int)(time * Cell::ticks_per_second + slice_time * 0.25); // offset makes sure rounding error doesn't cause round down into wrong frame
	//double frame_time = time - frame * frame_length;
	//int frame_step = (int)(frame_time / slice_time + 0.25);




	//Steps:
	// 0 = integrate acceleraton
	// 1 = update constraint targets
	// 2 = apply warming
	// for 0 <=k < constraint_iterations 
	// 3 + 2k = apply constraint
	//3 + 2k + 1 = collect impulses
	//3 + 2 * constrant_iterations = integrate velocity
	//3 + 2 * constrant_iterations + 1 to frame time  = update collisionsand find constraints
	
	//printf("running cell physics at %lf  frame:%d\n", time, frame);
	updateCollisions();
	
	//TODO collect impulses
	int next_step =  ((3 + 2 * Cell::constraint_iterations) + Cell::frame_slices)/2;
	queue(id, (frame+1) * frame_length + slice_time * next_step, &Cell::runPhysics);
}

void registerPhysics(){
	WorldPlugin* worlds = getTool<WorldPlugin>();
	worlds->registerClass<RigidBody, RigidBodyView>("RigidBody");
	worlds->registerMethod(&RigidBody::addConstraints, "RigidBody add constraints");
	worlds->registerMethod(&RigidBody::runPhysics, "RigidBody::runPhysics");
	
	
	worlds->registerClass<Cell>("Cell");
	worlds->registerMethod(&Cell::addBody,"addBody") ;
	worlds->registerMethod(&Cell::runPhysics, "Cell::runPhysics");

	worlds->registerClass<ManifoldCollision>("Collision manifold");
	worlds->registerMethod(&ManifoldCollision::addConstraint, "Manifold add constraint");
	worlds->registerMethod(&ManifoldCollision::runPhysics, "Collision::runPhysics");
	
}




} // end namespace Physics