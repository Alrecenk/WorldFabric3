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

	//TODO compute bounding radius
	bounding_radius = 0.5f;
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

BodyInstantData RigidBody::computeBodyInstant() const{
	BodyInstantData data ;
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
	double frame_slice = 0.1 ;
	double target_time = (frame_slice+frame) * frame_length;

	//If this function is off step then it's probably new (or maybe backed up)
	if(fabs(time-target_time)> frame_length *0.1){
		//printf("Miss-step, target: %lf\n", target_time) ;
		queue(id,target_time + frame_length,&RigidBody::runPhysics) ; // requeue on the step
		return ; // take no other action at this time
	}

	std::shared_ptr<const Root> root = read<Root>(root_id) ;
	
	//Incorporate constraints from the cells we're touching
	int amount = 0 ;
	glm::vec3 v_total(0,0,0) ;
	glm::vec3 av_total(0, 0, 0);
	for(int64_t& cell_id : cells){
		std::shared_ptr<const Cell> cell = read<Cell>(cell_id);
		if(cell){
			amount++;
			v_total += cell->constraint_result.at(id).first ;
			av_total += cell->constraint_result.at(id).second;
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

	//Check our cells and send updates iff they have changed
	std::vector<int64_t> new_cells = root->getCells(position, types[type].bounding_radius);

	for(int64_t& nc : new_cells){
		bool found = false;
		for (int64_t& c : cells) {
			found |= c == nc ;
		}
		if(!found){
			queue(nc,time, &Cell::addBody, id) ;
		}
	}
	for (int64_t& c : cells) {
		bool found = false;
		for (int64_t& nc : new_cells) {
			found |= c == nc;
		}
		if (!found) {
			queue(c, time, &Cell::removeBody, id);
		}
	}

	cells = new_cells ;

	//TODO determine if new cells are needed cand call update Cells on Root

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
		// TODO world_point = cell->nearestValidPosition(action->world, last_view, world_point, true);

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



//Returns an identifying hash that can be used to group this constraint into a set
int64_t Collision::getHash() const {
	return 0 ; // TODO
}

//Update the constraint target based on information at the start of the frame
void Collision::updateConstraint(RigidBody* body_1, RigidBody* body_2){
	// TODO
}

//Apply starting impulses carried over if the constraint has existed for multiple frames in a row
void Collision::applyWarmingImpulse(RigidBody* body_1, RigidBody* body_2){
	// TODO
}

//Applies impulses to velocity of involved bodies to attempt to satisfy the constraint
void Collision::applyConstraint(RigidBody* body_1, RigidBody* body_2){
	// TODO
}

//Retargets this constraint to the objects after they have moved
//Returns whether constraint is still valid
bool Collision::retargetConstraint(const RigidBody* body_1, const RigidBody* body_2){
	return false; // TODO
}


ManifoldCollision::ManifoldCollision(int64_t id1, int s1, int64_t id2, int s2){
	//TODO
}

//Returns an identifying hash that can be used to group constraints into this set
int64_t ManifoldCollision::getHash() const {
	return 0; // TODO
}


//Add a constraint to this set
void ManifoldCollision::addConstraint(Constraint& new_constraint){
	// TODO
}

//Update the constraint targets based on information at the start of the frame
//Returns if any of the constraints are active at all
void ManifoldCollision::updateConstraints(RigidBody* body_1, RigidBody* body_2){
	// TODO
}

//Apply starting impulses carried over if any constraint has existed for multiple frames in a row
void ManifoldCollision::applyWarmingImpulses(RigidBody* body_1, RigidBody* body_2){
	// TODO
}

//Applies impulses to velocity of involved bodies to satisfy these constraints
void ManifoldCollision::applyConstraints(RigidBody* body_1, RigidBody* body_2){
	// TODO
}

//Walks through state machine to run each physics step in lockstep with other elements
void Cell::runPhysics(){
	printf("cell run physics called!\n");
	//TODO
}

//Adds a body to the cell
void Cell::addBody(const int64_t& body_id){
	printf("cell addbody called!\n");
	//TODO

}

//Removes a body to the cell
void Cell::removeBody(const int64_t& body_id) {
	printf("cell removebody called!\n");
	//TODO

}

//Returns the nearest position to the target that the given body could be placed without colliding
//with the contents of this cell from the perspective of the active vantage point in the given world
//Note: This is for user controls that move bodies and is for use only OUTSIDE world events (like from views or actions)
glm::vec3 Cell::nearestValidPosition(const std::string& world, std::shared_ptr<const RigidBody>& body, const glm::vec3& target_position, bool immoveable_only) const{
	return glm::vec3(0,0,0) ; //TODO
}


//return the ids of all cells overlapping the bounding box of the given bounding sphere
std::vector<int64_t> Root::getCells(const glm::vec3& center, const float& radius) const{
	return std::vector<int64_t>() ; // TODO
}

//create any cells necessary to cover the given bounding box
void Root::updateCells(const glm::vec3& center, const float& radius){
	//TODO
	printf("Root updateCells called!\n");
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