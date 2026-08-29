#ifndef _PHYSICS_H_
#define _PHYSICS_H_ 1

#include "local_ptr.h"
#include "Polygon.h"

namespace Physics {

class ConvexShape{
public:
	float inv_mass = 0;
	glm::mat3 inv_moment = glm::mat3(0) ;
	float mass = 0 ;
	glm::mat3 moment = glm::mat3(0);

	//Returns the point on the shape furthest in the given direction
	virtual glm::vec3 support(const glm::vec3& direction) const = 0;
	
	//Returns the t for closest intersection on the ray p + v*t
	//Returns a negative number if the ray does not intersect
	virtual float rayTrace(const glm::vec3& p, const glm::vec3& v) const = 0;

	//Returns an axis aligned bounding box for the shape if it had the given pose
	//First element is min values, second is max values
	virtual std::pair<glm::vec3, glm::vec3> getAABB(const glm::mat4& pose) const = 0;

};


class ConvexPolyhedron : public ConvexShape {
public:
	std::vector<glm::vec3> vertex;
	std::vector<std::vector<int>> face;

	ConvexPolyhedron() {} 

	// If mass is not specified then the object has infinite mass and is immoveable
	ConvexPolyhedron(const std::vector<glm::vec3>& vertices, const std::vector<std::vector<int>>& faces);

	ConvexPolyhedron(const std::vector<glm::vec3>& vertices, const std::vector<std::vector<int>>& faces, float mass);

	// Make a new polyhedron by reposing and/or changing the mass of an existing polyhedron
	ConvexPolyhedron(ConvexPolyhedron& base, glm::mat4& pose, float mass);

	// Make a new polyhedron by reposing another, no specified mass means infinite mass and unmoveable
	ConvexPolyhedron(ConvexPolyhedron& base, glm::mat4& pose);

	void buildFromPolygons(std::vector<Polygon>& polygons);

	// If mass is not specified then the object has infinite mass and is immoveable
	ConvexPolyhedron(std::vector<Polygon>& polygons);

	ConvexPolyhedron(std::vector<Polygon>& polygons, float mass);


	// Return the center of mass of this shape
	glm::vec3 getCentroid();

	// Moves this shape so the origin aligns with the centroid and returns the move that was made
	glm::vec3 centerOnCentroid();

	float getVolume() ;

	glm::mat3 computeInertia(const float mass);

	//Returns the point on the shape furthest in the given direction
	glm::vec3 support(const glm::vec3& direction) const override;

	//Returns the t for closest intersection on the ray p + v*t
	//Returns a negative number if the ray does not intersect
	float rayTrace(const glm::vec3& p, const glm::vec3& v) const override;

	//Returns an axis aligned bounding box for the shape if it had the given pose
	//First element is min values, second is max values
	std::pair<glm::vec3, glm::vec3> getAABB(const glm::mat4& pose) const override;

	// Returns an axis aligned bounding box
	static ConvexPolyhedron makeAxisAlignedBox(glm::vec3 min, glm::vec3 max);

	//Alternate form of box that always centers on the origin
	static ConvexPolyhedron makeAxisAlignedBox(glm::vec3 size);

	// Returns a shape for a cylinder with center of ends A and B and the given radius and side count
	static ConvexPolyhedron makeCylinder(glm::vec3 A, glm::vec3 B, float radius, int sides);

	// Returns a shape for a Tetrahedron with the given points
	static ConvexPolyhedron makeTetra(glm::vec3 A, glm::vec3 B, glm::vec3 C, glm::vec3 D);

	// Returns an axis aligned bounding box
	static ConvexPolyhedron makeAxisAlignedBox(glm::vec3 min, glm::vec3 max, float mass);

	//Alternate form of box that always centers on the origin
	static ConvexPolyhedron makeAxisAlignedBox(glm::vec3 size, float mass);

	// Returns a shape for a cylinder with center of ends A and B and the given radius and side count
	static ConvexPolyhedron makeCylinder(glm::vec3 A, glm::vec3 B, float radius, int sides, float mass);

	// Returns a shape for a Tetrahedron with the given points
	static ConvexPolyhedron makeTetra(glm::vec3 A, glm::vec3 B, glm::vec3 C, glm::vec3 D, float mass);

	// Builds an approximate convex hull of the given model with up to the given number of faces
	// Detail level is sphere extrapolation used, it improves the quality but also increases the time taken exponentially
	static ConvexPolyhedron makeApproximateHull(std::shared_ptr<GLTF>& model, float mass, int hull_faces=30, int detail_level = 4) ;

	//Same as above but first breaks a model into its connected pieces
	static std::vector<ConvexPolyhedron> makeApproximateSurfaceHulls(std::shared_ptr<GLTF>& model, float mass, int hull_faces=30, int detail_level=4) ;

	//Collect the convex pieces of a model
	static std::vector<ConvexPolyhedron>collectConvexPiecesByBSP(std::shared_ptr<GLTF>& model);

	static std::vector<ConvexPolyhedron>collectConvexPiecesByRadialVolumes(std::shared_ptr<GLTF>& model, int depth);

	static std::vector<ConvexPolyhedron>collectConvexPiecesByBone(std::shared_ptr<GLTF>& model, int hull_faces = 20, int detail_level = 4, float min_weight = 0.3f, float min_bone_volume = 0);
};


class Sphere : public ConvexShape {
public:
	float radius ;

	Sphere(float radius, float mass) ;

	//Returns the point on the shape furthest in the given direction
	glm::vec3 support(const glm::vec3& direction) const override;

	//Returns the t for closest intersection on the ray p + v*t
	//Returns a negative number if the ray does not intersect
	float rayTrace(const glm::vec3& p, const glm::vec3& v) const override;

	//Returns an axis aligned bounding box for the shape if it had the given pose
	//First element is min values, second is max values
	std::pair<glm::vec3, glm::vec3> getAABB(const glm::mat4& pose) const override;
} ;


class RigidBody {
public:
	int64_t id ;
	glm::vec3 position = glm::vec3(0,0,0) ;
	glm::vec3 velocity = glm::vec3(0, 0, 0);
	glm::quat orientation = glm::quat(1, 0, 0, 0);
	glm::vec3 angular_velocity = glm::vec3(0, 0, 0);
	glm::mat4 pose = glm::mat4(1);
	glm::mat4 inv_pose = glm::mat4(1);

	std::vector<std::shared_ptr<ConvexShape>> shape ; // TODO add support for non-convex shapes by compounding
	float elasticity = 0.6f;
	float friction = 0.6f ;
	float drag = 0.05f ;
	float angular_drag = 0.05f ;

	//Inervse inertia and axis aligned bounding box in world space
	float inv_mass = 0;
	glm::mat3 base_inv_moment ;
	glm::mat3 inv_moment ;
	std::pair<glm::vec3, glm::vec3> AABB;

	RigidBody(const std::shared_ptr<ConvexShape>& s);

	RigidBody(const std::shared_ptr<ConvexShape>& s, int64_t i , const glm::vec3& p, const glm::vec3& v, const glm::vec3& w);


	RigidBody(const std::vector<std::shared_ptr<ConvexShape>>& s, int64_t i, const glm::vec3& p, const glm::vec3& v, const glm::vec3& w);

	void integrateVelocity(float dt);

	void integrateAcceleration(const glm::vec3& acceleration, float dt);

	void setPose(const glm::mat4& p){
		pose = p ;
		inv_pose = glm::inverse(p);
		orientation = glm::quat_cast(pose);
		position = p * glm::vec4(0,0,0,1);
	}
};


class PhysicsContainer{
public:
	virtual RigidBody* getBody(int64_t id) = 0 ;
};

class Constraint {
public:

	//Returns an identifying hash that can be used to group this constraint into a set
	virtual int64_t getHash() const = 0;

	//Update the constraint target based on information at the start of the frame
	//Returns if the constraint is active at all
	virtual bool updateConstraint(PhysicsContainer* cell) = 0;

	//Apply a starting impulse carried over if this constraint has existed for multiple frames in a row
	virtual void applyWarmingImpulse(PhysicsContainer* cell) = 0;

	//Applies impulse to velocity of involved bodies to satisfy this constraint
	virtual void applyConstraint(PhysicsContainer* cell) = 0;
};

class ConstraintSet{
public:

	//Returns an identifying hash that can be used to group constraints into this set
	virtual int64_t getHash() const = 0 ;
	
	//Add a constraint to this set
	virtual void addConstraint(PhysicsContainer* cell, Constraint& new_constraint) = 0 ;

	//Update the constraint targets based on information at the start of the frame
	//Returns if any of the constraints are active at all
	virtual bool updateConstraints(PhysicsContainer* cell) = 0;

	//Apply starting impulses carried over if any constraint has existed for multiple frames in a row
	virtual void applyWarmingImpulses(PhysicsContainer* cell) = 0;

	//Applies impulses to velocity of involved bodies to satisfy these constraints
	virtual void applyConstraints(PhysicsContainer* cell) = 0;

};


//Point in minkowski difference space
struct SupportPoint {
	glm::vec3 x;
	// hold onto points on shapes for use in subsequent steps
	glm::vec3 a, b;

	//Overload linear operators to allow manipulation in barycentric coordinates
	SupportPoint operator*(const float& scale) {
		return { x * scale, a * scale, b * scale };
	}

	SupportPoint operator+(const SupportPoint& o) {
		return { x + o.x, a + o.a, b + o.b };
	}
};

//A triangle in monkowski space with a set winding order
struct SupportTriangle {
	SupportPoint A;
	SupportPoint B;
	SupportPoint C;
	glm::vec3 normal; // normal should be normalize(cross(B - A, C - A))
	float d = 0; // normal * x + d > 0 means in front of the plane

	SupportTriangle(const SupportPoint& a, const  SupportPoint& b, const  SupportPoint& c) : A(a), B(b), C(c) {
		normal = glm::normalize(glm::cross(B.x - A.x, C.x - A.x));
		d = -glm::dot(normal, A.x);
	}

	float signedDistance(const glm::vec3& p) {
		return glm::dot(normal, p) + d;
	}
};

//We use edges to build out expanding polytope as points are added
struct SupportEdge {
	SupportPoint A;
	SupportPoint B;
	bool disabled = false;
};


class Collision : public Constraint {
public:
	int64_t id1 = -1;
	int shape1 = -1 ;
	int64_t id2 = -1;
	int shape2 = -1 ;
	glm::vec3 warm_impulse;
	glm::vec3 warm_tangent_impulse;
	std::vector<glm::vec3> tangents;
	glm::vec3 point; // middle point of collision
	glm::vec3 normal; // normal points from object 1 to object 2, doesn ot change
	glm::vec3 local_a ; // point on surface of a in A's local coordinates
	glm::vec3 local_b ; // point on surface of b in B's local coordinates
	float penetration_depth = 0;
	float target = 0;
	

	static inline const int CONSTRAINT_TYPE = 1 ;
	static inline float penetration_spring_coefficient = 10.0f;
	static inline float allowed_collision_depth = 0.02f;
	static inline float min_velocity_for_elastic = 0.1f;
	static inline float retarget_normal_alignment_minimum = 0.95f ;

	static int64_t getHash(int64_t id1, int s1, int64_t id2, int s2, int constraint_type) {
		return hashBytes(serialize(id1,s1, id2,s2, constraint_type));
	}


	int64_t getHash() const override;


	bool updateConstraint(PhysicsContainer* cell) override;
	void applyWarmingImpulse(PhysicsContainer* cell) override;
	void applyConstraint(PhysicsContainer* cell) override;

	//Retargets this constraint to the objects after they have moved
	//Returns whether constraint is still valid
	bool retargetConstraint(PhysicsContainer* cell) ;
};

//A simple collision that uses a single point and does not maintain a manifold
class SinglePointCollision : public ConstraintSet {
public:
	Collision point;

	//Returns an identifying hash that can be used to group constraints into this set
	int64_t getHash() const override;

	//Add a constraint to this set
	void addConstraint(PhysicsContainer* cell, Constraint& new_constraint) override;

	//Update the constraint targets based on information at the start of the frame
	//Returns if any of the constraints are active at all
	bool updateConstraints(PhysicsContainer* cell) override;

	//Apply starting impulses carried over if any constraint has existed for multiple frames in a row
	void applyWarmingImpulses(PhysicsContainer* cell) override;

	//Applies impulses to velocity of involved bodies to satisfy these constraints
	void applyConstraints(PhysicsContainer* cell) override;

};

//A simple collision that uses a single point and does not maintain a manifold
class ManifoldCollision : public ConstraintSet {
public:
	int64_t hash ;
	std::vector<Collision> points;
	static inline float squared_distance_for_match = 1e-5f ;
	static inline int max_collision_points = 4 ;

	ManifoldCollision(int64_t h): hash(h){};

	//Returns an identifying hash that can be used to group constraints into this set
	int64_t getHash() const override;

	//Add a constraint to this set
	void addConstraint(PhysicsContainer* cell, Constraint& new_constraint) override;

	//Update the constraint targets based on information at the start of the frame
	//Returns if any of the constraints are active at all
	bool updateConstraints(PhysicsContainer* cell) override;

	//Apply starting impulses carried over if any constraint has existed for multiple frames in a row
	void applyWarmingImpulses(PhysicsContainer* cell) override;

	//Applies impulses to velocity of involved bodies to satisfy these constraints
	void applyConstraints(PhysicsContainer* cell) override;

};

//Returns if two axis aligned bounding boxes intersect
bool AAABIntersect(const std::pair<glm::vec3, glm::vec3>& A, const std::pair<glm::vec3, glm::vec3>& B);

// return the volume of the given tetrahedron
float computeTetraVolume(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d);

// Returns the center of mass of the given tetrahedron assuming uniform density
glm::vec3 computeTetraCentroid(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d);

// Returns the inertia tensor of the given tetrahedron about the origin assuming a uniform density
glm::mat3 computeTetraInertia(const float mass, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d);


//Find the support point of the minkowski difference of two shapes
//Saves the points on the shapes for later reconstruction
SupportPoint findSupportPoint(const glm::vec3 direction, const RigidBody* A,int shapeA, const RigidBody* B, int shapeB);

//Build a support simplex from a triangle facing a point
std::vector<SupportTriangle> buildSupportSimplex(const SupportTriangle& triangle, const SupportPoint& D);
//Same as above but builds it into an existing simplex to avoid memory allocation
void buildSupportSimplex(const SupportTriangle triangle, const SupportPoint& D, std::vector<SupportTriangle>& into);

//Uses GJK to detect whether two convex shapes collide
//If they collide this returns a simplex in Minkowski diference space enclosing the collision point
//If they do not collide, this returns an empty vector
std::vector<SupportTriangle> detectCollision(const RigidBody* A, int shapeA, const RigidBody* B, int shapeB, int max_iterations = 10);

//Adds an edge fromed by the two support points to an edge list or disables an inner edge on duplication (used in getPenetration)
void countEdge(const SupportPoint& A, const SupportPoint& B, std::vector<SupportEdge>& edge_list);

//Uses expanding polytope algorithm on result of detectCollision
// Returns a supportPoint containg the resoltuion vector in x and the closets points on the shapes in a and b
SupportPoint getPenetration(std::vector<SupportTriangle>& collision_result, const RigidBody* A, int shapeA, const RigidBody* B, int shapeB, int max_iterations = 10);

class SimpleLocalPhysicsCell : PhysicsContainer {
public:

	class ObjectType {
	public:
		std::vector<std::shared_ptr<Physics::ConvexShape>> shape;
		std::string model;
		glm::mat4 render_transform;
		float elasticity;
		float friction;
	};

	//Bounding box of cell
	glm::vec3 acceleration = glm::vec3(0, -10, 0);

	//Contents of cell
	std::unordered_map<int64_t, std::shared_ptr<Physics::RigidBody>> bodies;
	std::unordered_map<int64_t, std::shared_ptr<Physics::ConstraintSet>> constraints;
	std::unordered_map<int, ObjectType> types;

	std::unordered_map<int64_t, std::pair<int, int>> instance; // maps physics objects to type and scene instance

	int next_object_id = 1;
	int next_type_id = 1;

	SimpleLocalPhysicsCell();

	//Custom destructor cleans up scene instance
	~SimpleLocalPhysicsCell();

	int addType(std::shared_ptr<Physics::ConvexShape> shape, const std::string& model, glm::mat4& render_transform, float elasticity = 0.5f, float friction = 0.5f);


	int addType(std::vector<std::shared_ptr<Physics::ConvexShape>> shape, const std::string& model, glm::mat4& render_transform, float elasticity = 0.5f, float friction = 0.5f);


	int addType(std::vector<Physics::ConvexPolyhedron> raw_shape, const std::string& model, glm::mat4& transform, float elasticity, float friction);

	int64_t add(int type, const glm::vec3& pos, const glm::vec3& vel = glm::vec3(0), const glm::vec3& a_vel = glm::vec3(0));

	//Ball ids are allocated one after another and are always positive
	Physics::RigidBody* getBody(int64_t id) override;

	//Constraint id is a hash generated with getConstraintID
	Physics::ConstraintSet* getConstraintSet(int64_t id);


	//Finds all collisions of the balls with each other and the walls of the cell
	//Creates or destroys constraints so the contents of constraints matches the current collisions
	//Also sets points and normal for collisions
	void updateCollisions();

	//Run physics forward one frame
	void runPhysicsFrame(float dt, int constraints_iter);

	//Uses types to render all objects with the scene plugin
	void updateGraphics();

	//Sets the pose of an object
	void setPose(int64_t id, const glm::mat4& pose);

};






} // end namespace physics

#endif // #ifndef _PHYSICS_H_