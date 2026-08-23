#include "Physics.h"

namespace Physics{



void RigidBody::integrateVelocity(float dt){
	if(shape->inv_mass == 0 && (glm::length(velocity) > 0 || glm::length(angular_velocity) > 0)){
		printf("Immovable is moving!\n");
	}
	if(dt <= 0){
		printf("Incorrect dt: %f\n", dt);
	}

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
}

void RigidBody::integrateAcceleration(const glm::vec3& acceleration, float dt){
	if (shape->inv_mass <= 0) { // don't accelerate objects with infinite mass
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


ConvexPolyhedron::ConvexPolyhedron(const std::vector<glm::vec3>& vertices, const std::vector<std::vector<int>>& faces) {
	vertex = vertices;
	face = faces;
	// TODO compute moment and mass n creation
}

// Returns a shape for an axis aligned bounding box
ConvexPolyhedron ConvexPolyhedron::makeAxisAlignedBox(glm::vec3 min, glm::vec3 max) {
	std::vector<glm::vec3> vertices;
	vertices.emplace_back(min.x, min.y, min.z); // 0
	vertices.emplace_back(max.x, min.y, min.z); // 1
	vertices.emplace_back(min.x, max.y, min.z); // 2
	vertices.emplace_back(max.x, max.y, min.z); // 3
	vertices.emplace_back(min.x, min.y, max.z); // 4
	vertices.emplace_back(max.x, min.y, max.z); // 5
	vertices.emplace_back(min.x, max.y, max.z); // 6
	vertices.emplace_back(max.x, max.y, max.z); // 7

	std::vector<std::vector<int>> faces;
	faces.push_back(std::vector<int>({ 0, 4, 6, 2 })); // min x
	faces.push_back(std::vector<int>({ 1, 3, 7, 5 })); // max x
	faces.push_back(std::vector<int>({ 0, 1, 5, 4 })); // min y
	faces.push_back(std::vector<int>({ 2, 6, 7, 3 })); // max y
	faces.push_back(std::vector<int>({ 0, 2, 3, 1 })); // min z
	faces.push_back(std::vector<int>({ 4, 5, 7, 6 })); // max z
	return ConvexPolyhedron(vertices, faces);
}

//Alternate form that always centers on the origin
ConvexPolyhedron ConvexPolyhedron::makeAxisAlignedBox(glm::vec3 size) {
	return makeAxisAlignedBox(size * -0.5f, size * 0.5f);
}

glm::vec3 ConvexPolyhedron::support(const glm::vec3& direction) const{
	float highest = -FLT_MAX;
	glm::vec3 support;
	//TODO walk edge graph to make this more efficient for more complex polyhedron
	for (auto& v : vertex) {
		float dot = glm::dot(v, direction);
		if (dot > highest) {
			highest = dot;
			support = v;
		}
	}
	return support;
}


//Returns the t for closest intersection on the ray p + v*t
//Returns a negative number if the ray does not intersect
float ConvexPolyhedron::rayTrace(const glm::vec3& p, const glm::vec3& v) const{
	return -1.0f ; //TODO
}

//Returns an axis aligned bounding box for the shape if it had the given pose
//First element is min values, second is max values
std::pair<glm::vec3, glm::vec3> ConvexPolyhedron::getAABB(const glm::mat4& pose) const {
	return std::pair<glm::vec3, glm::vec3>() ;//TODO
}

// Returns a shapefor a cylinder with center of ends and A and B
ConvexPolyhedron ConvexPolyhedron::makeCylinder(glm::vec3 A, glm::vec3 B, float radius, int sides) {
	glm::vec3 Z = B - A; // get axis_ along cylinder ((0,0,0) = A, (0,0,1) = B)
	glm::vec3 X = glm::normalize(glm::cross(glm::vec3(1, .8, .7), Z)) *
		radius; // Get an arbitrary axis orthogonal to Z
	glm::vec3 Y = glm::normalize(glm::cross(X, Z)) * radius; // Get final axis
	std::vector<glm::vec3> vertices;
	std::vector<std::vector<int>> faces;
	std::vector<int> top, bottom;
	const float twopi = 6.28318530718f;
	for (int side = 0; side < sides; side++) {
		float angle = side * twopi / sides;
		float dx = sin(angle);
		float dy = cos(angle);
		vertices.push_back(A + X * dx + Y * dy);
		vertices.push_back(B + X * dx + Y * dy);
		faces.emplace_back(
			std::vector<int>({ 2 * side + 1, 2 * side, (2 * side + 2) % (sides * 2), (2 * side + 3) % (sides * 2) }));
		top.push_back(side * 2 + 1);
		bottom.push_back((sides - 1 - side) * 2); // flip order for bottom face
	}
	//Top and bottom face
	faces.emplace_back(top);
	faces.emplace_back(bottom);
	return ConvexPolyhedron(vertices, faces);
}


// Returns a shape for a Tetrahedron with the given points
ConvexPolyhedron ConvexPolyhedron::makeTetra(glm::vec3 A, glm::vec3 B, glm::vec3 C, glm::vec3 D) {
	std::vector<glm::vec3> vertices;
	vertices.push_back(A);
	vertices.push_back(B);
	vertices.push_back(C);
	vertices.push_back(D);

	std::vector<std::vector<int>> faces;
	faces.push_back(std::vector<int>({ 0, 1, 2 }));
	faces.push_back(std::vector<int>({ 0, 1, 3 }));
	faces.push_back(std::vector<int>({ 0, 3, 2 }));
	faces.push_back(std::vector<int>({ 3, 1, 2 }));

	// Fix winding order so normals face out
	glm::vec3 center = (A + B + C + D) * 0.25f;
	for (int k = 0; k < faces.size(); k++) {
		glm::vec3& a = vertices[faces[k][0]];
		glm::vec3& b = vertices[faces[k][1]];
		glm::vec3& c = vertices[faces[k][2]];

		glm::vec3 n = glm::cross(b - a, c - a);
		if (glm::dot(a - center, n) < 0) {
			int t = faces[k][1];
			faces[k][1] = faces[k][2];
			faces[k][2] = t;
		}
	}
	return ConvexPolyhedron(vertices, faces);
}



int64_t Collision::getHash() const {
	return hashBytes(serialize(id1, id2, CONSTRAINT_TYPE));
}
bool Collision::updateConstraint(PhysicsContainer* cell){
	RigidBody* body_1 = cell->getBody(id1);
	RigidBody* body_2 = cell->getBody(id2);

	float velocity_against_normal = glm::dot(body_1->velocity - body_2->velocity, normal);

	float restitution_bias = 0.0f; // inelastic
	if (velocity_against_normal > min_velocity_for_elastic) {
		float e = (body_1->elasticity + body_2->elasticity) * 0.5f;
		restitution_bias = e * velocity_against_normal; // elastic
	}

	//Bias against penetration with spring force
	float penetration_bias = penetration_spring_coefficient * std::max(0.0f, glm::length(penetration.x) - allowed_collision_depth);

	target = restitution_bias + penetration_bias;
	return true ; // TODO compute if still relevant
}
void Collision::applyWarmingImpulse(PhysicsContainer* cell){
	RigidBody* body_1 = cell->getBody(id1);
	RigidBody* body_2 = cell->getBody(id2);

	body_1->velocity -= warm_impulse * body_1->shape->inv_mass;
	body_2->velocity += warm_impulse * body_2->shape->inv_mass;

	glm::vec3 r1 = point - body_1->position;
	glm::vec3 r2 = point - body_2->position;
	body_1->angular_velocity -= body_1->shape->inv_moment * glm::cross(r1, warm_impulse);// TODO inertia needs to be rotated based on pose of rigid body
	body_2->angular_velocity += body_2->shape->inv_moment * glm::cross(r2, warm_impulse);
}
void Collision::applyConstraint(PhysicsContainer* cell){
	RigidBody* body_1 = cell->getBody(id1);
	RigidBody* body_2 = cell->getBody(id2);

	//lever arms for torque
	glm::vec3 r1 = point - body_1->position;
	glm::vec3 r2 = point - body_2->position;


	glm::vec3 contact_velocity_1 = body_1->velocity + glm::cross(body_1->angular_velocity, r1);
	glm::vec3 contact_velocity_2 = body_2->velocity + glm::cross(body_2->angular_velocity, r2);
	glm::vec3 relative_velocity = contact_velocity_2 - contact_velocity_1;
	float velocity_along_normal = glm::dot(relative_velocity, normal);

	//calculate effective mass
	float rot_term1 = glm::dot(glm::cross(body_1->shape->inv_moment * glm::cross(r1, normal), r1), normal); // TODO inertia needs to be rotated based on pose of rigid body
	float rot_term2 = glm::dot(glm::cross(body_2->shape->inv_moment * glm::cross(r2, normal), r2), normal);
	float effective_mass_n = body_1->shape->inv_mass + body_2->shape->inv_mass + rot_term1 + rot_term2;
	if (effective_mass_n == 0.0f) {
		return; // two immovable objects
	}

	//Calculate current change needed based on already applied
	float impulse_mag_n = (target - velocity_along_normal) / effective_mass_n;
	float old_accumulated_n = glm::dot(warm_impulse, normal);
	float new_accumulated_n = std::max(0.0f, old_accumulated_n + impulse_mag_n);
	float current_impulse_n = new_accumulated_n - old_accumulated_n;
	glm::vec3 impulse_vec_n = normal * current_impulse_n;

	//Apply normal impulse
	body_1->velocity -= impulse_vec_n * body_1->shape->inv_mass;
	body_2->velocity += impulse_vec_n * body_2->shape->inv_mass;
	body_1->angular_velocity -= body_1->shape->inv_moment * glm::cross(r1, impulse_vec_n);// TODO inertia needs to be rotated based on pose of rigid body
	body_2->angular_velocity += body_2->shape->inv_moment * glm::cross(r2, impulse_vec_n);

	//update warm impulse
	warm_impulse += impulse_vec_n;

	// Recalculate velocities at point after normal impulse
	contact_velocity_1 = body_1->velocity + glm::cross(body_1->angular_velocity, r1);
	contact_velocity_2 = body_2->velocity + glm::cross(body_2->angular_velocity, r2);
	relative_velocity = contact_velocity_2 - contact_velocity_1;

	glm::vec3 tangent = relative_velocity - (glm::dot(relative_velocity, normal) * normal);
	float velocity_along_tangent = glm::length(tangent);

	if (velocity_along_tangent > 0.0001f) {
		tangent *= 1.0f / velocity_along_tangent; // normalize

		// Effective mass for tangent direction
		float rot_term1_t = glm::dot(glm::cross(body_1->shape->inv_moment * glm::cross(r1, tangent), r1), tangent);
		float rot_term2_t = glm::dot(glm::cross(body_2->shape->inv_moment * glm::cross(r2, tangent), r2), tangent); // TODO inertia needs to be rotated based on pose of rigid body
		float effective_mass_t = body_1->shape->inv_mass + body_2->shape->inv_mass + rot_term1_t + rot_term2_t;

		//Compute maximum tangent velocity ot be lost
		float impulse_mag_t = -1.0f * velocity_along_tangent / effective_mass_t;

		//Calculate current change needed based on already applied and clamp to fricton coefficient
		float max_friction = friction_coefficient * new_accumulated_n;
		float old_accumulated_t = glm::dot(warm_tangent_impulse, tangent);
		float new_accumulated_t = std::min(std::max(old_accumulated_t + impulse_mag_t, -max_friction), max_friction);
		float current_impulse_t = new_accumulated_t - old_accumulated_t;
		glm::vec3 impulse_vec_t = tangent * current_impulse_t;

		// Apply Tangent Impulse
		body_1->velocity -= impulse_vec_t * body_1->shape->inv_mass;
		body_2->velocity += impulse_vec_t * body_2->shape->inv_mass;
		body_1->angular_velocity -= body_1->shape->inv_moment * glm::cross(r1, impulse_vec_t);
		body_2->angular_velocity += body_2->shape->inv_moment * glm::cross(r2, impulse_vec_t);// TODO inertia needs to be rotated based on pose of rigid body

		//update warm impulse
		warm_tangent_impulse += impulse_vec_t;
	}

	if(std::isnan(glm::length(body_1->velocity)) || std::isnan(glm::length(body_1->angular_velocity)) || std::isnan(glm::length(body_2->velocity)) || std::isnan(glm::length(body_2->angular_velocity))){
		printf("nan in constraint!\n");
	}
	
}


//Returns an identifying hash that can be used to group constraints into this set
int64_t SinglePointCollision::getHash() const{
	return hashBytes(serialize(point.id1, point.id2, Collision::CONSTRAINT_TYPE));
}

//Add a constraint to this set
void SinglePointCollision::addConstraint(const Constraint& new_constraint){
	point = static_cast<const Collision&>(new_constraint) ;
}

//Update the constraint targets based on information at the start of the frame
//Returns if any of the constraints are active at all
bool SinglePointCollision::updateConstraints(PhysicsContainer* cell){
	point.updateConstraint(cell);
}

//Apply starting impulses carried over if any constraint has existed for multiple frames in a row
void SinglePointCollision::applyWarmingImpulses(PhysicsContainer* cell){
	point.applyWarmingImpulse(cell);
}

//Applies impulses to velocity of involved bodies to satisfy these constraints
void SinglePointCollision::applyConstraints(PhysicsContainer* cell){
	point.applyConstraint(cell);
}


Sphere::Sphere(float r, float m){
	radius = r ;
	inv_mass = 1.0f/ m ;
	inv_moment = glm::mat3(1.0f/ ( 0.4f * m * r * r)) ;
}

//Returns the point on the shape furthest in the given direction
glm::vec3 Sphere::support(const glm::vec3& direction) const{
	return glm::normalize(direction) * radius ;
}

//Returns the t for closest intersection on the ray p + v*t
//Returns a negative number if the ray does not intersect
float Sphere::rayTrace(const glm::vec3& p, const glm::vec3& v) const{
	return -1.0f ; // TODO
}

//Returns an axis aligned bounding box for the shape if it had the given pose
//First element is min values, second is max values
std::pair<glm::vec3, glm::vec3> Sphere::getAABB(const glm::mat4& pose) const {
	return std::pair<glm::vec3, glm::vec3>();//TODO
}


//Find the support point of the minkowski difference of two shapes
//Saves the points on the shapes for later reconstruction
SupportPoint findSupportPoint(const glm::vec3 direction, const RigidBody* A, const RigidBody* B) {
	SupportPoint sp;
	sp.a = A->pose * glm::vec4( A->shape->support(A->inv_pose* glm::vec4(direction,0)), 1);
	sp.b = B->pose * glm::vec4(B->shape->support(B->inv_pose * glm::vec4(-direction, 0)), 1);
	sp.x = sp.a - sp.b;
	return sp;
}


//Build a support simplex from a triangle facing a point
std::vector<SupportTriangle> buildSupportSimplex(const SupportTriangle& triangle, const SupportPoint& D) {
	std::vector<SupportTriangle> simplex;
	simplex.reserve(4);
	simplex.emplace_back(triangle.B, triangle.A, triangle.C); // flip initial triangle as outside is now inside
	simplex.emplace_back(D, triangle.B, triangle.C);
	simplex.emplace_back(triangle.A, D, triangle.C); // New triangles incorporating point and facing outward
	simplex.emplace_back(triangle.A, triangle.B, D);
	return simplex;
}

void buildSupportSimplex(const SupportTriangle triangle, const SupportPoint& D, std::vector<SupportTriangle>& simplex) {
	simplex[0] = SupportTriangle(triangle.B, triangle.A, triangle.C); // flip initial triangle as outside is now inside
	simplex[1] = SupportTriangle(D, triangle.B, triangle.C);
	simplex[2] = SupportTriangle(triangle.A, D, triangle.C); // New triangles incorporating point and facing outward
	simplex[3] = SupportTriangle(triangle.A, triangle.B, D);
}

//Uses GJK to detect whether two convex shapes collide
//If they collide this returns a simplex in Minkowski diference space enclosing the collision point
//If they do not collide, this returns an empty vector
std::vector<SupportTriangle> detectCollision(const RigidBody* A, const RigidBody* B) {
	// arbitrary first direction
	glm::vec3 search_direction = glm::vec3(1, 0, 0);
	const glm::vec3 origin(0, 0, 0);
	//std::vector<SupportPoint> p;
	SupportPoint p0 = findSupportPoint(search_direction, A, B);
	search_direction = origin - p0.x; // From p0 to origin
	SupportPoint p1 = findSupportPoint(search_direction, A, B);
	//New point could not get past zero in search direction
	if (glm::dot(p1.x, search_direction) <= 0) {
		return {}; // No collision
	}
	//Search perpendicular to p0 to p1 segment, toward origin
	search_direction = glm::cross(cross(p1.x - p0.x, search_direction), p1.x - p0.x);
	SupportPoint p2 = findSupportPoint(search_direction, A, B);
	if (glm::dot(p2.x, search_direction) <= 0) {
		return {}; // No collision
	}

	SupportTriangle first_triangle(p0, p1, p2);
	//Search along normal of triangle toward origin
	if (first_triangle.d < 0) { // facing wrong way to start
		first_triangle = SupportTriangle(p1, p0, p2); // flip winding order
	}

	search_direction = first_triangle.normal;
	SupportPoint p3 = findSupportPoint(search_direction, A, B);
	if (glm::dot(p3.x, search_direction) <= 0) {
		return {}; // No collision
	}

	std::vector<SupportTriangle> simplex = buildSupportSimplex(first_triangle, p3);

	for (int iter = 0; iter < MAX_GJK_ITERATIONS; iter++) {
		bool found_triangle = false;
		for (int k = 1; k < 4; k++) { // First triangle always what simpex was built from andwill always face out
			if (simplex[k].signedDistance(origin) > 0) {
				SupportPoint new_point = findSupportPoint(simplex[k].normal, A, B);
				//New point could not get past zero in search direction
				if (glm::dot(new_point.x, simplex[k].normal) <= 0) {
					return {}; // No collision
				}
				//simplex = buildSupportSimplex(simplex[k], new_point) ;
				buildSupportSimplex(simplex[k], new_point, simplex);
				found_triangle = true;
				break;
			}
		}
		if (!found_triangle) { // origin was insdide all faces
			return simplex; // collision detected
		}
	}
	return {};

}


void countEdge(const SupportPoint& A, const SupportPoint& B, std::vector<SupportEdge>& edge_list) {
	for (auto& edge2 : edge_list) {
		//order would be reversed on a duplicate
		//if (glm::distance2(edge2.A.x, B.x) < 1e-7f && glm::distance2(edge2.B.x, A.x) < 1e-7f) {
		if (edge2.A.x == B.x && edge2.B.x == A.x) {
			// edge occurs twice, don't build a new triangle
			edge2.disabled = true;
			return;
		}
	}
	edge_list.emplace_back(A, B);
}


//Uses expanding polytope algorithm on result of detectCollision
// Returns a supportPoint containg the resoltuion vector in x and the closets points on the shapes in a and b
SupportPoint getPenetration(std::vector<SupportTriangle>& collision_result, const RigidBody* A, const RigidBody* B) {
	static std::vector<SupportTriangle> polytope;
	static std::vector<SupportEdge> edge_list;
	polytope = collision_result;
	edge_list.clear();
	int iterations = 0;
	while (true) {

		//Find the nearest triangle on the polytope to the origin
		int selected_triangle = -1;
		float selected_distance = FLT_MAX;
		for (int k = 0; k < polytope.size(); k++) {
			if (-polytope[k].d < selected_distance) {
				selected_triangle = k;
				selected_distance = -polytope[k].d;
			}

		}
		SupportTriangle& active_face = polytope[selected_triangle];
		//use it's normal to expand to a new point
		SupportPoint new_point = findSupportPoint(active_face.normal, A, B);

		float signed_distance = active_face.signedDistance(new_point.x);
		//printf("D:%f\n", selected_distance) ;
		//Expansion didn't expand means we've reached closest surface face
		if (signed_distance < 1e-4f || iterations == MAX_GJK_ITERATIONS) {
			/*if(iterations ==MAX_GJK_ITERATIONS){
				printf("Hit max iterations in EPA!\n");
			}*/
			//printf("Finals SD:%f\n", signed_distance) ;
			glm::vec3 closest_x = active_face.normal * (-active_face.d); // closest point in minkowski space on plane
			//Get barycentric coordinates via area method
			glm::vec3 v0 = closest_x - active_face.A.x;
			glm::vec3 v1 = closest_x - active_face.B.x;
			glm::vec3 v2 = closest_x - active_face.C.x;
			float area_tot = glm::length(glm::cross(active_face.B.x - active_face.A.x, active_face.C.x - active_face.A.x));
			float a = glm::dot(glm::cross(v1, v2), active_face.normal) / area_tot;
			float b = glm::dot(glm::cross(v2, v0), active_face.normal) / area_tot;
			float c = 1.0f - a - b;

			//printf("a:%f, b:%f, c:%f\n", a,b,c) ;
			//printf(" %f == %f, %f == %f, %f == %f\n",collision_point.x.x, closest_x.x, collision_point.x.y, closest_x.y, collision_point.x.z, closest_x.z) ;
			SupportPoint collision_point = active_face.A * a + active_face.B * b + active_face.C * c;
			return collision_point;
		}

		//Remove all triangles facing the new point and collect their edges
		for (int k = 0; k < polytope.size(); k++) {
			if (polytope[k].signedDistance(new_point.x) > 1e-6f) {
				countEdge(polytope[k].A, polytope[k].B, edge_list);
				countEdge(polytope[k].B, polytope[k].C, edge_list);
				countEdge(polytope[k].C, polytope[k].A, edge_list);// Order of points matters here to make sure new triangles face outward

				//Remove it
				if (k != polytope.size() - 1) { // swap with final slot
					polytope[k] = polytope[polytope.size() - 1];
				}
				polytope.pop_back(); // remove final slot
				k--; // look at this slot again since we just moved something else into it
			}
		}

		//Add edges not duplicated
		for (auto& edge : edge_list) {
			if (!edge.disabled) {
				polytope.emplace_back(edge.A, edge.B, new_point);
			}
		}

		edge_list.clear();
		iterations++;
	}

}

} // end namespace Physics