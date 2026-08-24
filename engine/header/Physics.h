#ifndef _PHYSICS_H_
#define _PHYSICS_H_ 1

#include "local_ptr.h"

namespace Physics {

class ConvexShape{
public:
	float inv_mass = 0;
	glm::mat3 inv_moment = glm::mat3(0) ;

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

	ConvexPolyhedron(const std::vector<glm::vec3>& vertices, const std::vector<std::vector<int>>& faces);


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
	glm::quat orientation = glm::quat(0, 0, 0, 1);
	glm::vec3 angular_velocity = glm::vec3(0, 0, 0);
	glm::mat4 pose = glm::mat4(1);
	glm::mat4 inv_pose = glm::mat4(1);

	std::shared_ptr<ConvexShape> shape ; // TODO add support for non-convex shapes by compounding
	float elasticity = 0.6f;
	float friction = 0.6f ;
	float drag = 0.03f ;
	float angular_drag = 0.03f ;

	//Inervse inertia and axis aligned bounding box in world space
	glm::mat3 inv_moment ;
	std::pair<glm::vec3, glm::vec3> AABB;

	RigidBody(const std::shared_ptr<ConvexShape>& s);

	RigidBody(const std::shared_ptr<ConvexShape>& s, int64_t i , const glm::vec3& p, const glm::vec3& v, const glm::vec3& w);

	void integrateVelocity(float dt);

	void integrateAcceleration(const glm::vec3& acceleration, float dt);

	//Only for debugging, will be overwritten if physics is actually happening
	void setPose(const glm::mat4& p){
		pose = p ;
		inv_pose = glm::inverse(p);
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
	virtual void addConstraint(const Constraint& new_constraint) = 0 ;

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
	int64_t id2 = -1;
	glm::vec3 warm_impulse;
	glm::vec3 warm_tangent_impulse;
	std::vector<glm::vec3> tangents;
	glm::vec3 point; // middle point of collision
	glm::vec3 normal; // normal points from ball 1 to ball 2
	

	float target = 0;
	SupportPoint penetration ;
	

	static inline const int CONSTRAINT_TYPE = 1 ;
	static inline float penetration_spring_coefficient = 10.0f;
	static inline float allowed_collision_depth = 0.05f;
	static inline float min_velocity_for_elastic = 0.1f;

	int64_t getHash() const override;
	bool updateConstraint(PhysicsContainer* cell) override;
	void applyWarmingImpulse(PhysicsContainer* cell) override;
	void applyConstraint(PhysicsContainer* cell) override;
};

//A simple collision that uses a single point and does not maintain a manifold
class SinglePointCollision : public ConstraintSet {
	Collision point;

	//Returns an identifying hash that can be used to group constraints into this set
	int64_t getHash() const override;

	//Add a constraint to this set
	void addConstraint(const Constraint& new_constraint) override;

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

static inline const int MAX_GJK_ITERATIONS = 10;

//Find the support point of the minkowski difference of two shapes
//Saves the points on the shapes for later reconstruction
SupportPoint findSupportPoint(const glm::vec3 direction, const RigidBody* A, const RigidBody* B);

//Build a support simplex from a triangle facing a point
std::vector<SupportTriangle> buildSupportSimplex(const SupportTriangle& triangle, const SupportPoint& D);
//Same as above but builds it into an existing simplex to avoid memory allocation
void buildSupportSimplex(const SupportTriangle triangle, const SupportPoint& D, std::vector<SupportTriangle>& into);

//Uses GJK to detect whether two convex shapes collide
//If they collide this returns a simplex in Minkowski diference space enclosing the collision point
//If they do not collide, this returns an empty vector
std::vector<SupportTriangle> detectCollision(const RigidBody* A, const RigidBody* B);

//Adds an edge fromed by the two support points to an edge list or disables an inner edge on duplication (used in getPenetration)
void countEdge(const SupportPoint& A, const SupportPoint& B, std::vector<SupportEdge>& edge_list);

//Uses expanding polytope algorithm on result of detectCollision
// Returns a supportPoint containg the resoltuion vector in x and the closets points on the shapes in a and b
SupportPoint getPenetration(std::vector<SupportTriangle>& collision_result, const RigidBody* A, const RigidBody* B);

} // end namespace physics

#endif // #ifndef _PHYSICS_H_