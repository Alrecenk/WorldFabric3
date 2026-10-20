#include "NetPhysics2.h"
#include "ScenePlugin.h"

namespace NetPhysics2 {


BodyType::BodyType(std::vector<std::shared_ptr<Physics::ConvexShape>>& shape, const std::string& model, glm::mat4& render_transform, float elasticity, float friction){
	this->shape = shape ;
	this->model = model ;
	this->render_transform = render_transform ;
	this->elasticity = elasticity ;
	this->friction = friction ;

	float mass = 0;
	glm::mat3 moment(0);
	for (auto& part : shape) {
		mass += part->mass;
		moment += part->moment;
	}
	if (mass <= 0) { // immobile objects have 0 mass and inv_mass
		base_inv_moment = glm::mat3(0);
		inv_mass = 0;
	}else {
		base_inv_moment = glm::inverse(moment);
		inv_mass = 1.0f / mass;
	}

	bounding_radius = 0;
	for(auto& s : shape){
		bounding_radius = fmax(bounding_radius, s->boundingRadius()) ;
	}
	printf("Boudning radius: %f\n", bounding_radius) ;

}


//Create a rigid body from the static object type list on the RigidBodyView
RigidBody::RigidBody(int64_t root_id, int type, const glm::vec3& p, const glm::quat& o, const glm::vec3& vel, const glm::vec3& a_vel){
	this->type = type;
	this->root_id = root_id ;
	position = p;
	orientation = o ;
	velocity = vel,
	angular_velocity = a_vel;

	if(types[type].inv_mass == 0){
		receives_impulse = false;
	}
}

void RigidBody::integrateVelocity(float dt) {
	if (receives_impulse) { // don't integrate bodies that aren't running impulse physics

		float speed = glm::length(velocity);
		if (speed > types[type].max_speed) {
			velocity *= types[type].max_speed / speed;
		}

		float angular_speed = glm::length(angular_velocity);
		if (angular_speed > types[type].max_angular_speed) {
			angular_velocity *= types[type].max_angular_speed / angular_speed;
		}


		position += velocity * dt;
		//printf("moved: %f,%f,%f\n", velocity.x * dt, velocity.y * dt, velocity.z * dt) ;
		// Update orientation quaternion
		// dq/dt = 0.5 * omega * q
		glm::quat omega_quat(0, angular_velocity.x, angular_velocity.y, angular_velocity.z);
		orientation += (omega_quat * orientation) * (0.5f * dt);
		orientation = glm::normalize(orientation);

	}

}

BodyInstant RigidBody::computeBodyInstant() const{
	BodyInstant data ;
	data.body = std::make_shared<RigidBody>(*this);
	data.pose = glm::mat4(1.0f);
	data.pose = glm::translate(data.pose, position);
	data.pose = data.pose * glm::mat4_cast(orientation);
	data.inv_pose = glm::inverse(data.pose);

	glm::mat3 r = glm::mat3_cast(orientation);
	data.inv_moment = r * types[type].base_inv_moment * glm::transpose(r);
	data.AABB = { {FLT_MAX,FLT_MAX,FLT_MAX},{-FLT_MAX,-FLT_MAX,-FLT_MAX} };
	for (auto& s : types[type].shape) {
		auto  sAABB = s->getAABB(data.pose);
		data.AABB.first.x = fmin(data.AABB.first.x, sAABB.first.x);
		data.AABB.second.x = fmax(data.AABB.second.x, sAABB.second.x);
		data.AABB.first.y = fmin(data.AABB.first.y, sAABB.first.y);
		data.AABB.second.y = fmax(data.AABB.second.y, sAABB.second.y);
		data.AABB.first.z = fmin(data.AABB.first.z, sAABB.first.z);
		data.AABB.second.z = fmax(data.AABB.second.z, sAABB.second.z);
	}

	return data ;
}

void RigidBody::integrateAcceleration(float dt) {
	if (types[type].inv_mass <= 0 || !receives_impulse) { // don't accelerate objects with infinite mass
		return;
	}
	velocity += acceleration * dt;


	float speed = glm::length(velocity);
	if (speed < types[type].drag * dt) {
		velocity = glm::vec3(0, 0, 0);
	}
	else {
		velocity *= (speed - types[type].drag * dt) / speed;
	}

	float angular_speed = glm::length(angular_velocity);
	if (angular_speed < types[type].angular_drag * dt) {
		angular_velocity = glm::vec3(0, 0, 0);
	}
	else {
		angular_velocity *= (angular_speed - types[type].angular_drag * dt) / angular_speed;
	}

}

//Walks through state machine to run each physics step in lockstep with other elements
void RigidBody::runPhysics(){
	//printf("rigid body run physics %lf \n", time);
	double frame_length = 1.0 / Root::ticks_per_second;
	int frame = (int)(time * Root::ticks_per_second);
	double target_time = (frame + interframe_time) * frame_length;

	//If this function is off step then it's probably new (or maybe backed up)
	if(fabs(time-target_time)> frame_length *0.1){
		//printf("Miss-step, target: %lf\n", target_time) ;
		queue(id,target_time + frame_length,&RigidBody::runPhysics) ; // requeue on the step
		return ; // take no other action at this time
	}

	
	
	//Incorporate constraints from the cells we're touching
	int amount = 0 ;
	glm::vec3 v_total(0,0,0) ;
	glm::vec3 av_total(0, 0, 0);
	for(int64_t& cell_id : cells){
		std::shared_ptr<const Cell> cell = read<Cell>(cell_id);
		if(cell){
			auto iter = cell->constraint_result.find(id);
			if(iter != cell->constraint_result.end()){
				amount++;
				v_total += iter->second.first ;
				av_total += iter->second.second ;
			}
		}
	}
	if(amount > 0){
		velocity = v_total / amount ;
		angular_velocity = av_total/amount ;
	}

	
	//Move this object with its updated velocity
	float dt = 1.0f/Root::ticks_per_second ;

	//printf("actually running: %f\n", dt);
	integrateVelocity(dt);
	integrateAcceleration(dt);

	std::shared_ptr<const Root> root = read<Root>(root_id);
	if (root) {

		//Check our cells and send updates iff they have changed
		std::pair<std::vector<int64_t>, bool> new_cells = root->getCells(position, types[type].bounding_radius);

		for(int64_t& nc : new_cells.first){
			bool found = false;
			for (int64_t& c : cells) {
				found |= c == nc ;
			}
			if(!found){
				queue(nc,time + random() * frame_length * 0.1, &Cell::addBody, id) ;
			}
		}
		for (int64_t& c : cells) {
			bool found = false;
			for (int64_t& nc : new_cells.first) {
				found |= c == nc;
			}
			if (!found) {
				queue(c, time + random() * frame_length * 0.1, &Cell::removeBody, id);
			}
		}

		cells = new_cells.first ;

		if(new_cells.second){
			queue(root_id,time + random() * frame_length*0.5,&Root::updateCells, position, types[type].bounding_radius);
		}
	}

	//Requeue this event at the beginning of the next frame
	queue(id, target_time + frame_length, &RigidBody::runPhysics);
}

void RigidBody::setState(const glm::vec3& p, const glm::vec3& v, const glm::quat& o, const glm::vec3& av) {
	position = p;
	velocity = v;
	orientation = o;
	angular_velocity = av;
}

void RigidBody::setInteractions(const bool& receives, const bool& applies) {
	receives_impulse = receives;
	applies_impulse = applies;
}

int RigidBody::addType(std::vector<std::shared_ptr<Physics::ConvexShape>> shape, const std::string& model, glm::mat4& render_transform, float elasticity, float friction) {
	int id = next_type_id;
	next_type_id++;
	types[id] = BodyType(shape, model, render_transform, elasticity, friction);
	return id;
}

int RigidBody::addType(std::shared_ptr<Physics::ConvexShape> single_shape, const std::string& model, glm::mat4& render_transform, float elasticity, float friction){
	std::vector<std::shared_ptr<Physics::ConvexShape>> shape;
	shape.push_back(single_shape);
	return addType(shape, model, render_transform, elasticity, friction);
}

int RigidBody::addType(std::vector<Physics::ConvexPolyhedron> raw_shape, const std::string& model, glm::mat4& render_transform, float elasticity, float friction){
	std::vector<std::shared_ptr<Physics::ConvexShape>> shape;
	for (auto& s : raw_shape) {
		std::shared_ptr<Physics::ConvexPolyhedron> sh = std::make_shared<Physics::ConvexPolyhedron>(s, render_transform, s.mass);
		shape.push_back(sh);
	}
	return addType(shape, model, render_transform, elasticity, friction);
}



//created is called when an objectis observed that ws no observed last time view was called on the world
void RigidBodyView::created(std::shared_ptr<const RigidBody>& body) {
	ScenePlugin* scene = getTool<ScenePlugin>();
	ActionMap* action_map = getTool<ActionMap>();
	last_view = body;
	pose = glm::mat4(1.0f);
	pose = glm::translate(pose, body->position);
	pose = pose * glm::mat4_cast(body->orientation);
	pose = pose * RigidBody::types[body->type].render_transform;
	scene_id = scene->createInstance(RigidBody::types[body->type].model, pose);
	std::shared_ptr<GLTF> model = scene->getModelController(RigidBody::types[body->type].model);
	std::shared_ptr<ActionTrigger> trigger = std::shared_ptr<ActionTrigger>(new ActionTrigger(0, pose * glm::vec4(model->min, 1), pose * glm::vec4(model->max, 1), this));
	trigger_id = action_map->addTrigger(trigger);
}

//Update is called when an observation is made of an object that was also observed last frame on this same view
void RigidBodyView::updated(std::shared_ptr<const RigidBody>& body) {
	ScenePlugin* scene = getTool<ScenePlugin>();
	ActionMap* action_map = getTool<ActionMap>();
	WorldPlugin* worlds = getTool<WorldPlugin>();

	if (body->time <= last_view->time) { // sometimes the clock moves to stay in sync with the server and this can happen
		last_position = body->position;
		last_orientation = body->orientation;
		last_pose_time = body->time;

	}
	else {

		double observation_time = worlds->getWorldTime("netphysics", body->position); // TODO get world from view, view should know what world it's in, but it doesn't?
		float observation_age = (float)(observation_time - body->time);// time difference between what we're supposed to see and what we observed

		//Extrapolate the passage of time since the observation
		glm::vec3 view_position = body->position + body->velocity * observation_age;
		glm::quat omega_quat(0, body->angular_velocity.x, body->angular_velocity.y, body->angular_velocity.z);
		glm::quat view_orientation = body->orientation + (omega_quat * body->orientation) * (0.5f * observation_age);
		view_orientation = glm::normalize(view_orientation);

		float dt = (float)(observation_time - last_pose_time);

		float speed = glm::length(view_position - last_position) / dt;
		float max_speed = max_speed_base + max_speed_mult * std::max(glm::length(body->velocity), glm::length(last_view->velocity));

		if (speed >= max_speed) {
			float s = max_speed / speed;
			//printf("Interpolation active: %f > %f-> %f\n", speed, max_speed, s);
			view_position = interpolate(last_position, view_position, s);
			view_orientation = interpolate(last_orientation, view_orientation, s);
		}
		last_position = view_position;
		last_orientation = view_orientation;
		last_pose_time = observation_time;
	}

	pose = glm::mat4(1.0f);
	pose = glm::translate(pose, last_position);
	pose = pose * glm::mat4_cast(last_orientation);
	pose = pose * RigidBody::types[body->type].render_transform;

	last_view = body;

	scene->setPose(scene_id, pose);
	std::shared_ptr<GLTF> model = scene->getModelController(RigidBody::types[body->type].model);
	action_map->moveTrigger(trigger_id, pose * glm::vec4(model->min, 1), pose * glm::vec4(model->max, 1));
}

//Destroyed is called when an observation that was present in the last observation is no longer observed
//This view will be deleted immediately after this call (it's destructor will be called after this)
void RigidBodyView::destroyed() {
	ScenePlugin* scene = getTool<ScenePlugin>();
	ActionMap* action_map = getTool<ActionMap>();
	scene->deleteInstance(scene_id);
	action_map->removeTrigger(trigger_id);
}


void RigidBodyView::receiveAction(RayGrab* action, ActionTrigger* trigger) {
	if (action->held == -1 && last_view->receives_impulse) { // no piece currently held and this body is grabbable
		ScenePlugin* scene = getTool<ScenePlugin>();
		std::shared_ptr<GLTF> model = scene->getModelController(RigidBody::types[last_view->type].model);

		glm::mat4 scene_to_model_space = glm::inverse(pose);
		glm::vec3 model_origin = scene_to_model_space * glm::vec4(action->origin, 1); // positions have 1 in slot 4 to include translation
		glm::vec3 model_direction = scene_to_model_space * glm::vec4(action->direction, 0);

		float t = model->rayTrace(model_origin, model_direction);
		if (t > 0 && t < action->hover_depth) {// Only act if the actual model was hit and the closest
			action->hover = last_view->id;
			action->hover_depth = t;
			if (action->clicked) {
				action->next_held = last_view->id;
				action->active_item = trigger;
			}
		}
	}
}

void RigidBodyView::receiveSignal(int signal, RayGrab* action, ActionTrigger* trigger) {
	WorldPlugin* worlds = getTool<WorldPlugin>();
	if (signal == RayGrab::CLICKED) {
		//printf("grabbed: %lld\n", last_view->id);
		grab_distance = action->hover_depth;
		glm::vec3 world_point = action->origin + action->direction * grab_distance;
		grab_offset = last_view->position - world_point;
		last_grab_time = last_view->time;
		getTool<WorldPlugin>()->queue(action->world, last_view->id, &RigidBody::setInteractions, false, true);
	}
	else if (signal == RayGrab::RELEASED) {
		action->active_item = nullptr;
		action->next_held = -1;
		//printf("released: %lld\n", last_view->id) ;
		getTool<WorldPlugin>()->queue(action->world, last_view->id, &RigidBody::setInteractions, true, true);
	}
	else if (signal == RayGrab::UPDATED) {
		//printf("updating: %lld\n", last_view->id);

		glm::vec3 world_point = grab_offset + action->origin + action->direction * grab_distance;
		std::shared_ptr< const Cell> cell = worlds->observeNearest<Cell>(action->world); // TODO support mutlicell
		world_point = cell->nearestValidPosition(action->world, last_view, world_point, true);

		double dt = last_view->time - last_grab_time;

		if (dt > 1e-4f) {

			glm::vec3 v = (world_point - last_view->position) / dt;
			float speed = glm::length(v);
			if (speed > max_grab_velocity) {
				v *= max_grab_velocity / speed;
				world_point = last_view->position + v * dt;
			}
			worlds->queue(action->world, last_view->id, &RigidBody::setState, world_point, v, last_view->orientation, glm::vec3(0, 0, 0));
			last_grab_time = last_view->time;
		}
	}
}


//Update the constraint target based on information at the start of the frame
void Collision::updateConstraint(BodyInstant& body_1, BodyInstant& body_2){
	//lever arms for torque
	glm::vec3 r1 = point - body_1.body->position;
	glm::vec3 r2 = point - body_2.body->position;
	glm::vec3 contact_velocity_1 = body_1.body->velocity + glm::cross(body_1.body->angular_velocity, r1);
	glm::vec3 contact_velocity_2 = body_2.body->velocity + glm::cross(body_2.body->angular_velocity, r2);
	float velocity_against_normal = glm::dot(contact_velocity_1 - contact_velocity_2, normal);

	float restitution_bias = 0.0f; // inelastic
	if (velocity_against_normal > min_velocity_for_elastic) {
		float e = fmax(RigidBody::types[body_1.body->type].elasticity, RigidBody::types[body_2.body->type].elasticity);
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
}

//Apply starting impulses carried over if the constraint has existed for multiple frames in a row
void Collision::applyWarmingImpulse(BodyInstant& body_1, BodyInstant& body_2){

	warm_tangent_impulse -= normal * glm::dot(normal, warm_tangent_impulse);
	warm_impulse = normal * glm::dot(normal, warm_impulse);

	tangents.clear();
	glm::vec3 ref = (std::abs(normal.y) < 0.8f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
	tangents.push_back(glm::normalize(glm::cross(normal, ref)));
	tangents.push_back(glm::normalize(glm::cross(normal, tangents[0])));

	glm::vec3 impulse = warm_impulse + warm_tangent_impulse;

	//printf("Warm impulse: %f, %f, %f\n", impulse.x, impulse.y, impulse.z) ;

	body_1.body->velocity -= impulse * RigidBody::types[body_1.body->type].inv_mass;
	body_2.body->velocity += impulse * RigidBody::types[body_2.body->type].inv_mass;

	glm::vec3 r1 = point - body_1.body->position;
	glm::vec3 r2 = point - body_2.body->position;
	body_1.body->angular_velocity -= body_1.inv_moment * glm::cross(r1, impulse);
	body_2.body->angular_velocity += body_2.inv_moment * glm::cross(r2, impulse);
}

//Applies impulses to velocity of involved bodies to attempt to satisfy the constraint
void Collision::applyConstraint(BodyInstant& body_1, BodyInstant& body_2){
	//printf("Collision applying constraint\n");
	BodyType& body_1_type = RigidBody::types[body_1.body->type];
	BodyType& body_2_type = RigidBody::types[body_2.body->type];

	//lever arms for torque
	glm::vec3 r1 = point - body_1.body->position;
	glm::vec3 r2 = point - body_2.body->position;

	glm::vec3 contact_velocity_1 = body_1.body->velocity + glm::cross(body_1.body->angular_velocity, r1);
	glm::vec3 contact_velocity_2 = body_2.body->velocity + glm::cross(body_2.body->angular_velocity, r2);
	glm::vec3 relative_velocity = contact_velocity_2 - contact_velocity_1;
	float velocity_along_normal = glm::dot(relative_velocity, normal);

	//calculate effective mass
	float rot_term1 = glm::dot(glm::cross(body_1.inv_moment * glm::cross(r1, normal), r1), normal);
	float rot_term2 = glm::dot(glm::cross(body_2.inv_moment * glm::cross(r2, normal), r2), normal);
	float effective_mass_n = body_1_type.inv_mass + body_2_type.inv_mass + rot_term1 + rot_term2;
	if (effective_mass_n <= 1e-6f) {
		//printf("Collision early exit\n");
		return; // two immovable objects
	}

	//Calculate current change needed based on already applied
	float impulse_mag_n = (target - velocity_along_normal) / effective_mass_n;
	float old_accumulated_n = glm::dot(warm_impulse, normal);
	float new_accumulated_n = std::max(0.0f, old_accumulated_n + impulse_mag_n);
	float current_impulse_n = new_accumulated_n - old_accumulated_n;
	glm::vec3 impulse_vec_n = normal * current_impulse_n;

	//Apply normal impulse
	body_1.body->velocity -= impulse_vec_n * body_1_type.inv_mass;
	body_2.body->velocity += impulse_vec_n * body_2_type.inv_mass;
	body_1.body->angular_velocity -= body_1.inv_moment * glm::cross(r1, impulse_vec_n);
	body_2.body->angular_velocity += body_2.inv_moment * glm::cross(r2, impulse_vec_n);

	//update warm impulse
	warm_impulse += impulse_vec_n;

	// Recalculate velocities at point after normal impulse
	contact_velocity_1 = body_1.body->velocity + glm::cross(body_1.body->angular_velocity, r1);
	contact_velocity_2 = body_2.body->velocity + glm::cross(body_2.body->angular_velocity, r2);
	relative_velocity = contact_velocity_2 - contact_velocity_1;


	glm::vec3 tangent_impulse(0);
	for (auto& tangent : tangents) {
		float velocity_along_tangent = glm::dot(tangent, relative_velocity);

		// Effective mass for tangent direction
		float rot_term1_t = glm::dot(glm::cross(body_1.inv_moment * glm::cross(r1, tangent), r1), tangent);
		float rot_term2_t = glm::dot(glm::cross(body_2.inv_moment * glm::cross(r2, tangent), r2), tangent);
		float effective_mass_t = body_1_type.inv_mass + body_2_type.inv_mass + rot_term1_t + rot_term2_t;
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
		float max_friction = (body_1_type.friction + body_2_type.friction) * 0.5f * new_accumulated_n;
		float clamped_magnitude = std::min(friction_magnitude, max_friction);
		accumulated_friction *= clamped_magnitude / friction_magnitude;
	}
	tangent_impulse = accumulated_friction - warm_tangent_impulse;


	// Apply Tangent Impulse
	body_1.body->velocity -= tangent_impulse * body_1_type.inv_mass;
	body_2.body->velocity += tangent_impulse * body_2_type.inv_mass;
	body_1.body->angular_velocity -= body_1.inv_moment * glm::cross(r1, tangent_impulse);
	body_2.body->angular_velocity += body_2.inv_moment * glm::cross(r2, tangent_impulse);

	//printf("appled impulse!\n");
	//update warm impulse
	warm_tangent_impulse = accumulated_friction;

	glm::vec3 impulse = warm_impulse + warm_tangent_impulse;

	//printf("Warm impulse after apply: %f, %f, %f\n", impulse.x, impulse.y, impulse.z);
}

//Retargets this constraint to the objects after they have moved
//Returns whether constraint is still valid
bool Collision::retargetConstraint(const BodyInstant& body_1, const BodyInstant& body_2){
	glm::vec3 a = body_1.pose * glm::vec4(local_a, 1);
	glm::vec3 b = body_2.pose * glm::vec4(local_b, 1);
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


ManifoldCollision::ManifoldCollision(int64_t id1, int s1, int64_t id2, int s2){
	id_1 = id1 ;
	shape_1 = s1 ;
	id_2 = id2 ;
	shape_2 = s2 ;
}

//Returns an identifying hash that can be used to group constraints into this set
int64_t ManifoldCollision::getHash() const {
	return Collision::getHash(id_1, shape_1, id_2, shape_2);
}


//Add a constraint to this set
void ManifoldCollision::addConstraint(Collision& new_point, BodyInstant& body_1, BodyInstant& body_2){
	std::vector<int> to_keep;
	int closest = -1;
	float cd2 = FLT_MAX;
	bool needs_start = points.size() == 0;
	for (int k = 0; k < points.size(); k++) {
		bool valid = points[k].retargetConstraint(body_1, body_2);
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

	//printf("collision manifold updated to %d points\n", (int)points.size()) ;
}

//Update the constraint targets based on information at the start of the frame
//Returns if any of the constraints are active at all
void ManifoldCollision::updateConstraints(BodyInstant& body_1, BodyInstant& body_2){
	for (auto& p : points) {
		p.updateConstraint(body_1, body_2);
	}
}

//Apply starting impulses carried over if any constraint has existed for multiple frames in a row
void ManifoldCollision::applyWarmingImpulses(BodyInstant& body_1, BodyInstant& body_2){
	for (auto& p : points) {
		p.applyWarmingImpulse(body_1, body_2);
	}
}

//Applies impulses to velocity of involved bodies to satisfy these constraints
void ManifoldCollision::applyConstraints(BodyInstant& body_1, BodyInstant& body_2){
	//printf("manifold applying constraints to %d points\n", (int)points.size());
	for (auto& p : points) {
		p.applyConstraint(body_1, body_2);
	}
}


//Finds all collisions
//Creates or destroys constraints so the contents of constraints match the current collisions
//Also sets points and normals for collisions
void Cell::updateCollisions(std::map<int64_t, BodyInstant>& bodies){
	//printf("Updating collisions on %d bodies\n", (int)bodies.size()) ;
	std::set<int64_t> found_constraints;
	for (auto& [id1, body_1] : bodies) {
		BodyType& body_1_type = RigidBody::types[body_1.body->type] ;
		for (auto& [id2, body_2] : bodies) {
			BodyType& body_2_type = RigidBody::types[body_2.body->type];
			if (id1 < id2 && // only check each pair once
				//collision_disabled.find({ id1,id2 }) == collision_disabled.end() &&  // collision not explicitly disabled between this pair
				(body_1_type.inv_mass > 0 || body_2_type.inv_mass > 0) && // only check if one is moveable
				Physics::AAABIntersect(body_1.AABB, body_2.AABB)) { // check AABBs first
				//printf("AABBs intersect  %lld to %lld\n", id1, id2);
				for (int shape_1= 0; shape_1 < body_1_type.shape.size(); shape_1++) {
					for (int shape_2 = 0; shape_2 < body_2_type.shape.size(); shape_2++) {

						auto simplex = Physics::detectCollision(&body_1, body_1_type.shape[shape_1].get(), &body_2, body_2_type.shape[shape_2].get());
						if (simplex.size() > 0) {
							Physics::SupportPoint sp = Physics::getPenetration(simplex, &body_1, body_1_type.shape[shape_1].get(), &body_2, body_2_type.shape[shape_2].get());
							float penetration_depth = glm::length(sp.x) ;
							if (penetration_depth > Physics::Collision::allowed_collision_depth * 0.5f) {
								glm::vec3 point = (sp.a + sp.b) * 0.5f;
								glm::vec3 normal = glm::normalize(sp.x);
								normal = glm::normalize(normal);
								int64_t constraint_hash = Collision::getHash(id1, shape_1, id2, shape_2);

								Collision constraint;
								constraint.point = point;
								constraint.normal = normal;
								constraint.local_a = body_1.inv_pose * glm::vec4(sp.a, 1);
								constraint.local_b = body_2.inv_pose * glm::vec4(sp.b, 1);
								constraint.penetration_depth = penetration_depth;

								if (collisions.find(constraint_hash) == collisions.end()) {
									collisions[constraint_hash] = ManifoldCollision(id1, shape_1, id2, shape_2);
									//printf("Collision manifold created\n");
								}

								collisions[constraint_hash].addConstraint(constraint, body_1, body_2);
								found_constraints.insert(constraint_hash);

							}
						}
					}
				}
			}
		}

	}

	//Delete existing collisions not found now
	std::vector<int64_t> to_delete;
	for (auto& [id, constraint] : collisions) {
		if (found_constraints.find(id) == found_constraints.end()) {
			to_delete.push_back(id);
		}
	}
	for (auto& id : to_delete) {
		collisions.erase(id);
	}


}

//Walks through state machine to run each physics step in lockstep with other elements
void Cell::runPhysics(){
	//printf("cell run physics called!\n");
	double frame_length = 1.0 / Root::ticks_per_second;
	int frame = (int)(time * Root::ticks_per_second);
	double target_time = (interframe_time + frame) * frame_length;

	// If this function is off step then it's probably new (or maybe backed up)
	if (fabs(time - target_time) > frame_length * 0.1) {
		//printf("Miss-step, target: %lf\n", target_time) ;
		queue(id, target_time + frame_length, &RigidBody::runPhysics); // requeue on the step
		return; // take no other action at this time
	}

	std::map<int64_t, BodyInstant> read_bodies;
	for (const int64_t& id : bodies) {
		std::shared_ptr<const RigidBody> body = read<RigidBody>(id);
		if (body) {
			if(time - body->time > frame_length){
				printf("Body too old!\n");
			}
			read_bodies[id] = body->computeBodyInstant();
		}else{
			printf("body could not be read!\n");
		}
	}


	updateCollisions(read_bodies);
	
	for (auto& [id, constraint] : collisions) {
		auto iter1 = read_bodies.find(constraint.id_1);
		auto iter2 = read_bodies.find(constraint.id_2);
		if(iter1 != read_bodies.end() && iter2 != read_bodies.end()){
			constraint.updateConstraints(iter1->second, iter2->second);
		}
	}
	for (auto& [id, constraint] : collisions) {
		auto iter1 = read_bodies.find(constraint.id_1);
		auto iter2 = read_bodies.find(constraint.id_2);
		if (iter1 != read_bodies.end() && iter2 != read_bodies.end()) {
			constraint.applyWarmingImpulses(iter1->second, iter2->second);
		}
	}
	for (int i = 0; i < Root::constraint_iterations; i++) {
		//printf("Constraint iteration: %d\n", i) ;
		for (auto& [id, constraint] : collisions) {
			auto iter1 = read_bodies.find(constraint.id_1);
			auto iter2 = read_bodies.find(constraint.id_2);
			if (iter1 != read_bodies.end() && iter2 != read_bodies.end()) {
				constraint.applyConstraints(iter1->second, iter2->second);
			}
		}
	}

	//glm::vec3 h = glm::vec3(0,0,0) ;
	//int k = 0 ;
	for(auto& [id, body] : read_bodies){
		//Save results that real bodies want to read
		constraint_result[id] = {body.body->velocity, body.body->angular_velocity} ;
		//printf("result velocity: %f, %f, %f\n", body.body->velocity.x, body.body->velocity.y, body.body->velocity.z) ;
		/*
		if(fabs(body.body->velocity.y) > 0.001f){
			if(k == 0){
				h.x = body.body->velocity.y ;
			}
			if (k == 1) {
				h.y = body.body->velocity.y;
			}
			if (k == 2) {
				h.z = body.body->velocity.y;
			}
			k++;
		}
		*/
	}
	//position = h*0.001f;

	queue(id,target_time + frame_length,&Cell::runPhysics) ;
}

//Adds a body to the cell
void Cell::addBody(const int64_t& body_id){
	//printf("cell addbody called! %lld\n", body_id);
	bodies.insert(body_id);
}

//Removes a body to the cell
void Cell::removeBody(const int64_t& body_id) {
	//printf("cell removebody called! %lld\n", body_id);
	bodies.erase(body_id);
}

//Returns the nearest position to the target that the given body could be placed without colliding
//with the contents of this cell from the perspective of the active vantage point in the given world
//Note: This is for user controls that move bodies and is for use only OUTSIDE world events (like from views or actions)
glm::vec3 Cell::nearestValidPosition(const std::string& world, std::shared_ptr<const RigidBody>& test_body, const glm::vec3& target_position, bool immoveable_only) const{
	WorldPlugin* worlds = getTool<WorldPlugin>();
	std::shared_ptr<RigidBody> test_copy = static_pointer_cast<RigidBody>(test_body->deepCopy());
	test_copy->position = target_position;
	BodyInstant tester = test_copy->computeBodyInstant();
	
	std::map<int64_t, BodyInstant> read_bodies;
	for (const int64_t& id : bodies) {
		std::shared_ptr<const RigidBody> body = read<RigidBody>(id);
		if (body) {
			read_bodies[id] = body->computeBodyInstant();
		}
	}
	BodyType& tester_type = RigidBody::types[tester.body->type];

	for (auto& [id, body] : read_bodies) {
		if (body.body->id != tester.body->id &&
			(!immoveable_only || !body.body->receives_impulse) &&
			Physics::AAABIntersect(body.AABB, tester.AABB)) { // check AABBs first
			BodyType& body_type = RigidBody::types[body.body->type];

			//printf("AABBs are colliding\n");
			for (int shape_1 = 0; shape_1 < body_type.shape.size(); shape_1++) {
				for (int shape_2 = 0; shape_2 < tester_type.shape.size(); shape_2++) {

					auto simplex = Physics::detectCollision(&body, body_type.shape[shape_1].get(), &tester, tester_type.shape[shape_2].get());
					if (simplex.size() > 0) {
						Physics::SupportPoint sp = Physics::getPenetration(simplex, &body, body_type.shape[shape_1].get(), &tester, tester_type.shape[shape_2].get());
						
						tester.body->position += sp.x;
						tester = tester.body->computeBodyInstant();
					}
				}
			}
		}
	}

	return  tester.body->position;
}


Root::Root(float size){
	cell_size = size ;
}

//returns the ids of all cells overlapping the bounding box of the given bounding sphere
	//The second element of the return is whether the root needs to be updated (i.e. to create more cells)
	//If the second element is true for a rigid body then that rigid body should call updateCells with the same data
std::pair<std::vector<int64_t>, bool> Root::getCells(const glm::vec3& center, const float& radius) const{
		//printf("root get cells called\n");
		//TODO add support for more than one cell
		return std::pair<std::vector<int64_t>, bool>(cells, (cells.size() == 0));
}

//create any cells necessary to cover the given bounding box
void Root::updateCells(const glm::vec3& center, const float& radius){
	//printf("Root updateCells called\n");
	if(cells.size() == 0){
		std::shared_ptr<Cell> cell = std::make_shared<Cell>() ;
		int64_t cell_id = create(cell, time) ;
		cells.push_back(cell_id) ;
		
		double frame_length = 1.0 / Root::ticks_per_second;
		int frame = (int)(time * Root::ticks_per_second);
		double target_time = (frame + Cell::interframe_time) * frame_length;
		queue(cell_id, target_time + frame_length, &Cell::runPhysics);

		printf("Cell created: %lld!\n", cell_id);
	}
}

void registerPhysics(){
	WorldPlugin* worlds = getTool<WorldPlugin>();
	worlds->registerClass<RigidBody, RigidBodyView>("RigidBody");
	worlds->registerMethod(&RigidBody::runPhysics, "RigidBody::runPhysics");
	worlds->registerMethod(&RigidBody::setInteractions, "RigidBody::setInteractions");
	worlds->registerMethod(&RigidBody::setState, "RigidBody::setState");

	worlds->registerClass<Cell>("Cell");
	worlds->registerMethod(&Cell::addBody, "addBody");
	worlds->registerMethod(&Cell::removeBody, "removeBody");
	worlds->registerMethod(&Cell::runPhysics, "Cell::runPhysics");

	worlds->registerClass<Root>("Physics Root");
	worlds->registerMethod(&Root::updateCells, "Root::updateCells");

}

} // end namespace