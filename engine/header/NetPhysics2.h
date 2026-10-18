#ifndef _NET_PHYSICS2_H_
#define _NET_PHYSICS2_H_ 1

#include "Physics.h"
#include "WorldPlugin.h"
#include "ActionMap.h"


namespace NetPhysics2 {


//Information about a body type stored statically and used by multiple bodies
class BodyType {
public:
	std::vector<std::shared_ptr<Physics::ConvexShape>> shape;

	float inv_mass = 0;
	glm::mat3 base_inv_moment;
	float bounding_radius  = -1.0f;

	std::string model;
	glm::mat4 render_transform;

	float elasticity = 0.4f;
	float friction = 0.6f;
	float drag = 1.0f;
	float angular_drag = 0.01f;
	float max_speed = 40.0f;
	float max_angular_speed = 20.0f;


	BodyType(){};

	BodyType(std::vector<std::shared_ptr<Physics::ConvexShape>>& shape, const std::string& model, glm::mat4& render_transform, float elasticity, float friction);

};


class RigidBody : public WorldObject, public Physics::PosedBody {
public:
	
	int type = -1 ; // index into static type list
	glm::vec3 velocity = glm::vec3(0, 0, 0);
	glm::vec3 acceleration = glm::vec3(0, -10, 0);
	glm::quat orientation = glm::quat(1, 0, 0, 0);
	glm::vec3 angular_velocity = glm::vec3(0, 0, 0);
		
	int64_t root = -1; // links to root of physics system for finding cells
	std::vector<int64_t> cells ; // cells currently occupied


	bool receives_impulse = true;
	bool applies_impulse = true;

	RigidBody() {}


	//Create a rigid body from the static object type list on the RigidBodyView
	RigidBody(int64_t root_id, int type, const glm::vec3& p, const glm::quat& o = glm::quat(1, 0, 0, 0), const glm::vec3& vel = glm::vec3(0), const glm::vec3& a_vel = glm::vec3(0));


	//Walks through state machine to run each physics step in lockstep with other elements
	void runPhysics();

	void setState(const glm::vec3& p, const glm::vec3& v, const glm::quat& o, const glm::vec3& av);

	//Sets how this body interacts with others through constraints
	void setInteractions(const bool& receives, const bool& applies);

	//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
	// Just change the template parameter to match your class
	int getTypeId(Registry* r) const {
		return r->getIdForType<RigidBody>();
	}

	std::shared_ptr<WorldObject> deepCopy() const override {
		return std::make_shared<RigidBody>(*this);
	}

	//Functions used on observables or on read objects need to be const
	void print() const override {
		printf("RigidBody");
	}


	static inline std::unordered_map<int, BodyType> types;
	static inline int next_type_id = 1;


	static int addType(std::shared_ptr<Physics::ConvexShape> shape, const std::string& model, glm::mat4& render_transform, float elasticity = 0.5f, float friction = 0.5f);


	static int addType(std::vector<std::shared_ptr<Physics::ConvexShape>> shape, const std::string& model, glm::mat4& render_transform, float elasticity = 0.5f, float friction = 0.5f);


	static int addType(std::vector<Physics::ConvexPolyhedron> raw_shape, const std::string& model, glm::mat4& render_transform, float elasticity = 0.5f, float friction = 0.5f);

};

auto static getStructure(RigidBody& o) {
	return std::tie(o.type, o.position, o.velocity, o.acceleration, o.orientation, o.angular_velocity, o.root, o.cells, o.receives_impulse, o.applies_impulse) ;
}

class RigidBodyView : public ObjectView<RigidBody>, public ActionReceiver<RayGrab> {
public:


	int64_t id;
	int scene_id = -1;
	int trigger_id = -1;
	std::shared_ptr<const RigidBody> last_view;

	glm::vec3 grab_offset;
	double last_grab_time = 0;
	float grab_distance = 0;
	float max_grab_velocity = 20.0f;

	//Max speed = base + mult * max(speed last frame, speed this frame)
	static inline float max_speed_base = 0.1f;
	static inline float max_speed_mult = 1.5f;

	glm::mat4 pose;
	glm::vec3 last_position;
	glm::quat last_orientation;
	double last_pose_time = 0;

	//created is called when an objectis observed that ws no observed last time view was called on the world
	void created(std::shared_ptr<const RigidBody>& body) override;

	//Update is called when an observation is made of an object that was also observed last frame on this same view
	void updated(std::shared_ptr<const RigidBody>& body) override;

	//Destroyed is called when an observation that was present in the last observation is no longer observed
	//This view will be deleted immediately after this call (it's destructor will be called after this)
	void destroyed() override;

	~RigidBodyView() = default;

	void receiveAction(RayGrab* action, ActionTrigger* trigger) override;
	void receiveSignal(int signal, RayGrab* action, ActionTrigger* trigger) override;


};


class Constraint {
public:

	//Returns an identifying hash that can be used to group this constraint into a set
	virtual int64_t getHash() const = 0;

	//Update the constraint target based on information at the start of the frame
	virtual void updateConstraint(RigidBody* body_1, RigidBody* body_2) = 0;

	//Apply starting impulses carried over if the constraint has existed for multiple frames in a row
	virtual void applyWarmingImpulse(RigidBody* body_1, RigidBody* body_2) = 0;

	//Applies impulses to velocity of involved bodies to attempt to satisfy the constraint
	virtual void applyConstraint(RigidBody* body_1, RigidBody* body_2) = 0;
};

class ConstraintSet {
public:

	bool delete_if_not_updated = true; // collision constraints get autodeletedif not being actively updated
	double last_update_time = 0;

	//Returns an identifying hash that can be used to group constraints into this set
	virtual int64_t getHash() const = 0;

	//Add a constraint to this set
	virtual void addConstraint(Constraint& new_constraint) = 0;

	//Update the constraint targets based on information at the start of the frame
	//Returns if any of the constraints are active at all
	virtual void updateConstraints(RigidBody* body_1, RigidBody* body_2) = 0;

	//Apply starting impulses carried over if any constraint has existed for multiple frames in a row
	virtual void applyWarmingImpulses(RigidBody* body_1, RigidBody* body_2) = 0;

	//Applies impulses to velocity of involved bodies to satisfy these constraints
	virtual void applyConstraints(RigidBody* body_1, RigidBody* body_2) = 0;

};

class Collision : public Constraint{
public:

	glm::vec3 warm_impulse;
	glm::vec3 warm_tangent_impulse;
	std::vector<glm::vec3> tangents;
	glm::vec3 point; // middle point of collision
	glm::vec3 normal; // normal points from object 1 to object 2
	glm::vec3 local_a; // point on surface of a in A's local coordinates
	glm::vec3 local_b; // point on surface of b in B's local coordinates
	float penetration_depth = 0;
	float target = 0;

	glm::vec3 next_impulse; // The next impulse to be applied by tis constraint when checked byits bodies


	static inline const int CONSTRAINT_TYPE = 1;
	static inline float penetration_spring_coefficient = 2.0f;
	static inline float allowed_collision_depth = 0.06f;
	static inline float min_velocity_for_elastic = 0.1f;
	static inline float retarget_normal_alignment_minimum = 0.9f;

	static int64_t getHash(int64_t id1, int s1, int64_t id2, int s2) {
		return hashBytes(serialize(id1, s1, id2, s2, CONSTRAINT_TYPE));
	}


	//Returns an identifying hash that can be used to group this constraint into a set
	virtual int64_t getHash() const override;

	//Update the constraint target based on information at the start of the frame
	virtual void updateConstraint(RigidBody* body_1, RigidBody* body_2) override;

	//Apply starting impulses carried over if the constraint has existed for multiple frames in a row
	virtual void applyWarmingImpulse(RigidBody* body_1, RigidBody* body_2) override;

	//Applies impulses to velocity of involved bodies to attempt to satisfy the constraint
	virtual void applyConstraint(RigidBody* body_1, RigidBody* body_2) override;

	//Retargets this constraint to the objects after they have moved
	//Returns whether constraint is still valid
	bool retargetConstraint(const RigidBody* body_1, const RigidBody* body_2);
};




auto static getStructure(Collision& o) {
	return std::tie(o.warm_impulse, o.warm_tangent_impulse, o.tangents, o.point, o.normal, o.local_a, o.local_b, o.penetration_depth, o.target, o.next_impulse);
}

//A simple collision that uses a single point and does not maintain a manifold
class ManifoldCollision : public ConstraintSet {
public:
	int64_t hash = -1;

	int64_t id_1 = -1;
	char shape_1 = -1;
	int64_t id_2 = -1;
	char shape_2 = -1;

	std::vector<Collision> points;

	static inline float squared_distance_for_match = 1e-4f;
	static inline int max_collision_points = 4;
	static inline int manifold_iterations = 1;
	static inline float relaxation = 0.99f;


	ManifoldCollision() {}

	ManifoldCollision(int64_t id1, int s1, int64_t id2, int s2);

	//Returns an identifying hash that can be used to group constraints into this set
	int64_t getHash() const override;


	//Add a constraint to this set
	void addConstraint(Constraint& new_constraint) override;

	//Update the constraint targets based on information at the start of the frame
	//Returns if any of the constraints are active at all
	void updateConstraints(RigidBody* body_1, RigidBody* body_2) override;

	//Apply starting impulses carried over if any constraint has existed for multiple frames in a row
	void applyWarmingImpulses(RigidBody* body_1, RigidBody* body_2) override;

	//Applies impulses to velocity of involved bodies to satisfy these constraints
	void applyConstraints(RigidBody* body_1, RigidBody* body_2) override;

};

auto static getStructure(ManifoldCollision& o) {
	return std::tie( o.id_1, o.shape_1, o.id_2, o.shape_2, o.last_update_time, o.hash, o.points);
}



//Runs physics pdates in a local region
class Cell : public WorldObject {
public:
	int64_t root ;
	float size = -1; // Cell occupies a box centered on position with axis aligned radius of size
	std::vector<int64_t> bodies;
	std::map<int64_t, ManifoldCollision> collisions; // maps constraint hash to constraint
	
	std::map<int64_t, std::pair<glm::vec3, glm::vec3>> constraint_result ; // maps body id to velo9city and angular velocity of constraint reslt

	static inline int ticks_per_second = 120;
	static inline int constraint_iterations = 6;
	static inline int frame_slices = 20;

	Cell() {};

	//Walks through state machine to run each physics step in lockstep with other elements
	void runPhysics();

	//Adds a body to the cell
	void addBody(const int64_t& body_id) ;

	//Returns the nearest position to the target that the given body could be placed without colliding
	//with the contents of this cell from the perspective of the active vantage point in the given world
	//Note: This is for user controls that move bodies and is for use only OUTSIDE world events (like from views or actions)
	glm::vec3 nearestValidPosition(const std::string& world, std::shared_ptr<const RigidBody>& body, const glm::vec3& target_position, bool immoveable_only) const;


	//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
	// Just change the template parameter to match your class
	int getTypeId(Registry* r) const {
		return r->getIdForType<Cell>();
	}

	std::shared_ptr<WorldObject> deepCopy() const override {
		return std::make_shared<Cell>(*this);
	}

	//Functions used on observables or on read objects need to be const
	void print() const override {
		printf("PhysicsCell");
	}

};

auto static getStructure(Cell& o) {
	return std::tie(o.position, o.root, o.size, o.bodies, o.collisions, o.constraint_result);
}

//Contains cells and routes body requests to the appropriate one
class Root: public WorldObject {
public:
	float cell_size = -1 ;
	std::vector<int64_t> cells ; // TODO make some sort of map for quicker lookups

	Root(){};
	Root(float size) ;

	//return the ids of all cells overlapping the bounding box of the given bounding sphere
	std::vector<int64_t> getCells(const glm::vec3& center, const float& radius) const ;

	//create any cells necessary to cover the given bounding box
	void updateCells(const glm::vec3& center, const float& radius) ;


	//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
	// Just change the template parameter to match your class
	int getTypeId(Registry* r) const {
		return r->getIdForType<Root>();
	}

	std::shared_ptr<WorldObject> deepCopy() const override {
		return std::make_shared<Root>(*this);
	}

	//Functions used on observables or on read objects need to be const
	void print() const override {
		printf("Physics Root");
	}
};

auto static getStructure(Root& o) {
	return std::tie(o.position, o.cell_size, o.cells);
}

void registerPhysics();

} // end namespace 

#endif // #ifndef _NET_PHYSICS2_H_