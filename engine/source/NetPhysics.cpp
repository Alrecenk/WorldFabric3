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

void RigidBody::integrateAcceleration(const glm::vec3& acceleration, float dt){
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


} // end namespace Physics