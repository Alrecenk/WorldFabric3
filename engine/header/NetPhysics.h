#ifndef _NET_PHYSICS_H_
#define _NET_PHYSICS_H_ 1

#include "Physics.h"
#include "WorldPlugin.h"
#include "local_ptr.h"




namespace NetPhysics {


//Shapeset contains a variety of explicitly typed shapes
//Designed to be cmpatible with local_ptr
class ShapeSet {
public:
	std::vector<Physics::Sphere> sphere;
	std::vector<Physics::ConvexPolyhedron> poly;
	//Add more shapes here later


	void addShape(const std::shared_ptr<Physics::ConvexShape>& shape) {
		std::shared_ptr<const Physics::Sphere> s = dynamic_pointer_cast<const Physics::Sphere>(shape);
		if (s) {
			sphere.emplace_back(*(s.get()));
		}
		std::shared_ptr<const Physics::ConvexPolyhedron> p = dynamic_pointer_cast<const Physics::ConvexPolyhedron>(shape);
		if (p) {
			poly.emplace_back(*(p.get()));
		}
	}

	ShapeSet(){}

	ShapeSet(const std::shared_ptr<Physics::ConvexShape>& shape) {
		addShape(shape);
	}

	ShapeSet(const std::vector<std::shared_ptr<Physics::ConvexShape>>& shapes){
			for (auto& shape : shapes) {
				addShape(shape);
			}
	}

	

	//Convenience functions for iterator
	int getBucketCount() const { return 2; }
	int getBucketSize(int bucket) const {
		switch (bucket) {
		case 0: return (int)sphere.size();
		case 1: return (int)poly.size();
		default: return 0;
		}
	}
	const Physics::ConvexShape& getShape(int bucket, int index) const {
		switch (bucket) {
		case 0: return sphere[index];
		case 1: return poly[index];
		default: throw std::out_of_range("Invalid ShapeSet fetch");
		}
	}

	Physics::ConvexShape& getShape(int bucket, int index){
		switch (bucket) {
		case 0: return sphere[index];
		case 1: return poly[index];
		default: throw std::out_of_range("Invalid ShapeSet fetch");
		}
	}

	//Iterator and begin and end defintions allow C++17 style loops for const ShapeSet&
	class Iterator {
	public:

	private:
		const ShapeSet* set ;
		int bucket=0;
		int index=0;
		const Physics::ConvexShape* current ;

		void advance(){
			while (bucket < set->getBucketCount() && index >= set->getBucketSize(bucket)) {
				bucket++;
				index = 0;
			}
			if (bucket < set->getBucketCount()) {
				current = &set->getShape(bucket, index);
			} else {
				current = nullptr;
			}
		}
	public:
		Iterator(const ShapeSet* s, int b, int i) : set(s), bucket(b), index(i) {
			advance();
		}

		Iterator& operator++() {
			index++;
			advance();
			return *this;
		}

		Iterator operator++(int) {
			Iterator tmp = *this;
			++(*this);
			return tmp;
		}

		auto& operator*() const { return *current; }
		auto* operator->() const { return current; }

		bool operator==(const Iterator& other) const { return current == other.current; }
		bool operator!=(const Iterator& other) const { return current != other.current; }
	};

	Iterator begin() const { return Iterator(this, 0, 0); }
	Iterator end() const { return Iterator(this, getBucketCount(), 0); }

	// Explicit const entry points (modern C++ style)
	Iterator cbegin() const { return Iterator(this, 0, 0); }
	Iterator cend() const { return Iterator(this, getBucketCount(), 0); }
	
};

auto static getStructure(ShapeSet& o ){
	return std::tie(o.sphere, o.poly) ;
}

class RigidBody : public WorldObject, public Physics::PosedBody {
public:
	glm::vec3 velocity = glm::vec3(0, 0, 0);
	glm::vec3 acceleration = glm::vec3(0, -10, 0);
	glm::quat orientation = glm::quat(1, 0, 0, 0);
	glm::vec3 angular_velocity = glm::vec3(0, 0, 0);
	//glm::mat4 pose = glm::mat4(1);
	//glm::mat4 inv_pose = glm::mat4(1);

	local_ptr<ShapeSet> shape ;
	float elasticity = 0.6f;
	float friction = 0.6f ;
	float drag = 0.25f ;
	float angular_drag = 0.25f ;

	//Inervse inertia and axis aligned bounding box in world space
	float inv_mass = 0;
	glm::mat3 base_inv_moment ;
	glm::mat3 inv_moment ;
	std::pair<glm::vec3, glm::vec3> AABB;

	int render_type = 0 ;

	std::vector<int64_t> constraints ;


	RigidBody(){}

	RigidBody(local_ptr<ShapeSet>& s, int64_t i, const glm::vec3& p, const glm::vec3& v, const glm::vec3& w);

	//Create a rigid body from the static object type list on the RigidBodyView
	RigidBody(int view_type,const glm::vec3& p, const glm::vec3& vel = glm::vec3(0), const glm::vec3& a_vel = glm::vec3(0)) ;

	void integrateVelocity(float dt);

	void integrateAcceleration(float dt);

	void applyConstraintImpulses() ;

	void addConstraints(const std::vector<int64_t>& new_constraints);

	void setPose(const glm::mat4& p){
		pose = p ;
		inv_pose = glm::inverse(p);
		orientation = glm::quat_cast(pose);
		position = p * glm::vec4(0,0,0,1);
		velocity = glm::vec3(0);
		angular_velocity = glm::vec3(0) ;
	}

	//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
	// Just change the template parameter to match your class
	int getTypeId(Registry* r) const {
		return r->getIdForType<RigidBody>();
	}

	//Functions used on observables or on read objects need to be const
	void print() const override{
		printf("RigidyBody");
	}

	//Walks through state machine to run each physics step in lockstep with other elements
	void runPhysics() ;
};


auto static getStructure(RigidBody& o){
	return std::tie(o.position, o.velocity, o.acceleration, o.orientation, o.angular_velocity, o. render_type, o.shape, o.constraints,
		o.elasticity, o.friction, o.drag, o.angular_drag, o.inv_mass, o.base_inv_moment, // TODO these could be grouped into a local_ptr to reduce network load
		o.pose, o.inv_pose, o.inv_moment, o.AABB) ; // TODO the could be computed with onDeserialize to reduce network load
}

class RigidBodyView : public ObjectView<RigidBody> {
public:


	int64_t id;
	int scene_id = -1;
	std::shared_ptr<const RigidBody> last_view;

	//created is called when an objectis observed that ws no observed last time view was called on the world
	void created(std::shared_ptr<const RigidBody>& body) override;

	//Update is called when an observation is made of an object that was also observed last frame on this same view
	void updated(std::shared_ptr<const RigidBody>& body) override;

	//Destroyed is called when an observation that was present in the last observation is no longer observed
	//This view will be deleted immediately after this call (it's destructor will be called after this)
	void destroyed() override;

	~RigidBodyView() = default;


	class ObjectType {
	public:
		local_ptr<NetPhysics::ShapeSet> shape;
		std::string model;
		glm::mat4 render_transform;
		float elasticity;
		float friction;
	};

	static inline std::unordered_map<int, ObjectType> types;
	static inline int next_type_id = 1 ;


	static int addType(std::shared_ptr<Physics::ConvexShape> shape, const std::string& model, glm::mat4& render_transform, float elasticity = 0.5f, float friction = 0.5f);


	static int addType(std::vector<std::shared_ptr<Physics::ConvexShape>> shape, const std::string& model, glm::mat4& render_transform, float elasticity = 0.5f, float friction = 0.5f);


	static int addType(std::vector<Physics::ConvexPolyhedron> raw_shape, const std::string& model, glm::mat4& render_transform, float elasticity, float friction);
};


class Constraint {
public:

	//Returns an identifying hash that can be used to group this constraint into a set
	virtual int64_t getHash() const = 0;

	//Update the constraint target based on information at the start of the frame
	virtual void updateConstraint(WorldObject* owner) = 0;

	//Updates the next impulse to be applied by while iterating the constraint
	virtual void setConstraintImpulse(WorldObject* owner) = 0;
};

class ConstraintSet : public WorldObject{
public:

	bool delete_if_not_updated = true; // collision constraints get autodeletedif not being actively updated
	double last_update_time = 0 ;

	//Returns an identifying hash that can be used to group constraints into this set
	virtual int64_t getHash() const = 0;

	//Update the constraint targets based on information at the start of the frame
	//Returns if any of the constraints are active at all
	virtual void updateConstraints() = 0;

	//Applies impulses to velocity of involved bodies to satisfy these constraints
	virtual void setConstraintImpulses() = 0;

};

class Collision : public Constraint {
public:
	int64_t id1 = -1;
	int shape1 = -1;
	int64_t id2 = -1;
	int shape2 = -1;
	glm::vec3 warm_impulse;
	glm::vec3 warm_tangent_impulse;
	std::vector<glm::vec3> tangents;
	glm::vec3 point; // middle point of collision
	glm::vec3 normal; // normal points from object 1 to object 2
	glm::vec3 local_a; // point on surface of a in A's local coordinates
	glm::vec3 local_b; // point on surface of b in B's local coordinates
	float penetration_depth = 0;
	float target = 0;

	glm::vec3 next_impulse ; // The next impulse to be applied by tis constraint when checked byits bodies


	static inline const int CONSTRAINT_TYPE = 1;
	static inline float penetration_spring_coefficient = 1.0f;
	static inline float allowed_collision_depth = 0.03f;
	static inline float min_velocity_for_elastic = 0.1f;
	static inline float retarget_normal_alignment_minimum = 0.95f;

	static int64_t getHash(int64_t id1, int s1, int64_t id2, int s2) {
		return hashBytes(serialize(id1, s1, id2, s2, CONSTRAINT_TYPE));
	}


	int64_t getHash() const override;
	void updateConstraint(WorldObject* owner) override;
	void setConstraintImpulse(WorldObject* owner) override;

	//Retargets this constraint to the objects after they have moved
	//Returns whether constraint is still valid
	bool retargetConstraint(WorldObject* owner);
};

auto static getStructure(Collision& o) {
	return std::tie(o.id1,o.shape1,o.id2,o.shape2,o.warm_impulse, o.warm_tangent_impulse, o.tangents, o.point, o.normal, o.local_a, o.local_b, o.penetration_depth, o.target, o.next_impulse) ;
}

//A simple collision that uses a single point and does not maintain a manifold
class ManifoldCollision : public ConstraintSet {
public:
	int64_t hash = -1;
	std::vector<Collision> points;
	static inline float squared_distance_for_match = 1e-5f;
	static inline int max_collision_points = 4;

	ManifoldCollision(){}

	ManifoldCollision(int64_t h) : hash(h) {};

	//Returns an identifying hash that can be used to group constraints into this set
	int64_t getHash() const override;

	//Add a constraint to this set
	void addConstraint(const Collision& new_point);

	//Update the constraint targets based on information at the start of the frame
	//Returns if any of the constraints are active at all
	void updateConstraints() override;

	//Applies impulses to velocity of involved bodies to satisfy these constraints
	void setConstraintImpulses() override;

	void runPhysics();

	//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
	// Just change the template parameter to match your class
	int getTypeId(Registry* r) const {
		return r->getIdForType<ManifoldCollision>();
	}

	//Functions used on observables or on read objects need to be const
	void print() const override {
		printf("ManifoldCollision");
	}
};

auto static getStructure(ManifoldCollision& o) {
	return std::tie(o.position, o.last_update_time, o.hash, o.points);
}

class Cell : public WorldObject {
public:
	std::vector<int64_t> bodies ;
	std::map<int64_t,int64_t> constraints ; // maps constraint hash to world ID of constraint set	
	static inline int ticks_per_second = 60 ;
	static inline int constraint_iterations = 4 ;
	static inline int frame_slices = 20;

	Cell(){};

	//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
	// Just change the template parameter to match your class
	int getTypeId(Registry* r) const {
		return r->getIdForType<Cell>();
	}

	//Functions used on observables or on read objects need to be const
	void print() const override {
		printf("PhysicsCell");
	}

	void addBody(const int64_t& new_body) ;

	void updateCollisions();

	void runPhysics() ;
};

auto static getStructure(Cell& o) {
	return std::tie(o.position, o.bodies);
}


void registerPhysics() ;

} // end namespace physics

#endif // #ifndef _PHYSICS_H_