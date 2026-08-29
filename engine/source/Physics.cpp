#include "Physics.h"
#include "ScenePlugin.h"
#include "VolumeNode.h"
#include "BSPNode.h"
#include <stack>

namespace Physics{


	RigidBody::RigidBody(const std::shared_ptr<ConvexShape>& s) {
		shape = {s};
		base_inv_moment = s->inv_moment;
		inv_mass = s->inv_mass;
	}

	RigidBody::RigidBody(const std::shared_ptr<ConvexShape>& s, int64_t i, const glm::vec3& p, const glm::vec3& v, const glm::vec3& w) {
		shape = {s};
		id = i;
		position = p;
		velocity = v;
		angular_velocity = w;
		base_inv_moment = s->inv_moment ;
		inv_mass = s->inv_mass ;
	}

	RigidBody::RigidBody(const std::vector<std::shared_ptr<ConvexShape>>& s, int64_t i, const glm::vec3& p, const glm::vec3& v, const glm::vec3& w){
		shape = s ;
		id = i;
		position = p;
		velocity = v;
		angular_velocity = w;

		float mass = 0;
		glm::mat3 moment(0) ;
		for(auto& part : shape){
			mass+= part->mass ;
			moment += part->moment ;
		}
		if(mass <=0){ // immobile objects have 0 mass and inv_mass
			base_inv_moment = glm::mat3(0);
			inv_mass = 0 ;
		}else{
			base_inv_moment = glm::inverse(moment);
			inv_mass = 1.0f/ mass ;
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
		auto  sAABB = s->getAABB(pose);
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

ConvexPolyhedron::ConvexPolyhedron(const std::vector<glm::vec3>& vertices, const std::vector<std::vector<int>>& faces) {
	vertex = vertices;
	face = faces;
	inv_mass = 0 ;
	inv_moment = glm::mat3(0);

}

ConvexPolyhedron::ConvexPolyhedron(const std::vector<glm::vec3>& vertices, const std::vector<std::vector<int>>& faces, float mass) {
	vertex = vertices;
	face = faces;
	this->mass = mass;
	inv_mass = 1.0f / mass;
	moment = computeInertia(mass) ;
	inv_moment = glm::inverse(moment);
}

ConvexPolyhedron::ConvexPolyhedron(ConvexPolyhedron& base, glm::mat4& pose, float mass) {
	vertex = base.vertex;
	for(auto& v : vertex){
		 v = pose * glm::vec4(v,1.0f) ;
	}

	face = base.face;
	if (mass > 0 && mass < 1e7) {
		this->mass = mass;
		inv_mass = 1.0f / mass;
		moment = computeInertia(mass);
		inv_moment = glm::inverse(moment);
	}
}

ConvexPolyhedron::ConvexPolyhedron(ConvexPolyhedron& base, glm::mat4& pose) {
	vertex = base.vertex;
	for (auto& v : vertex) {
		v = pose * glm::vec4(v, 1.0f);
	}

	face = base.face;
}


void ConvexPolyhedron::buildFromPolygons(std::vector<Polygon>& polygons){
	//deduplicate the points to match reduced shape format
	vertex = std::vector<glm::vec3>();
	face = std::vector<std::vector<int>>();
	glm::dvec3 mid ;
	for (int k = 0; k < polygons.size(); k++) {
		Polygon& poly = polygons[k];
		if (poly.p.size() >= 3) {
			std::vector<int> f;
			for (int j = 0; j < poly.p.size(); j++) {
				int index = -1;
				for (int i = 0; i < vertex.size(); i++) { // TODO could make this much faster
					if (glm::length(poly.p[j] - glm::dvec3(vertex[i])) < Polygon::EPSILON) {
						index = i;
						break;
					}
				}
				if (index == -1) {
					glm::vec3 v = poly.p[j];
					vertex.push_back(v);
					index = (int)(vertex.size() - 1);
					mid+= poly.p[j];
				}
				f.push_back(index);
			}
			face.push_back(f);
		}
	}
	
	mid/= vertex.size();
	//flip any faces pointing inward (Polygon doesn't have a winding oder guarantee)
	for (int k = 0; k < face.size(); k++) {
		//printf("fs:%d\n",(int)face.size() );
		glm::vec3& A = vertex[face[k][0]];
		glm::vec3& B = vertex[face[k][1]];
		glm::vec3& C = vertex[face[k][2]];
		//printf("ABC:%d,%d,%d, size:%d\n",face[k][0],face[k][1],face[k][2], (int)vertex.size() );
		glm::vec3 normal = glm::normalize(glm::cross(B - A, C - A));
		float d= -glm::dot(normal, A) ;
		if (glm::dot(normal, glm::vec3(mid)) + d > 0) {
			std::vector<int> new_face;
			for (int j = (int)(face[k].size() - 1); j >= 0; j--) {
				new_face.push_back(face[k][j]);
			}
			face[k] = new_face;
		}
	}

}

ConvexPolyhedron::ConvexPolyhedron(std::vector<Polygon>& polygons) {
	buildFromPolygons(polygons) ;
	inv_mass = 0;
	inv_moment = glm::mat3(0);
}

ConvexPolyhedron::ConvexPolyhedron(std::vector<Polygon>& polygons, float mass) {
	buildFromPolygons(polygons);
	if(mass > 0 && mass < 1e7){
		this->mass =mass ;
		inv_mass = 1.0f / mass;
		moment = computeInertia(mass);
		inv_moment = glm::inverse(moment);
	}
}

// Return the center of mass of this shape
glm::vec3 ConvexPolyhedron::getCentroid() {
	// Get a point on the inside
	glm::vec3 inner_point(0, 0, 0);
	for (const auto& v : vertex) {
		inner_point += v;
	}
	inner_point /= vertex.size();
	glm::vec3 centroid(0, 0, 0);
	float  volume = 0;
	for (const auto& f : face) {
		for (int k = 1; k < f.size() - 1; k++) {
			glm::vec3& a = vertex[f[0]];
			glm::vec3& b = vertex[f[k]];
			glm::vec3& c = vertex[f[k + 1]];
			glm::vec3& d = inner_point;
			float vol = computeTetraVolume(a, b, c, d);
			glm::vec3 ctr = computeTetraCentroid(a, b, c, d);
			volume += vol;
			centroid += ctr * vol;
		}
	}
	return centroid / volume;

}

// Moves this shape so the origin aligns with the centroid and returns the move that was made
glm::vec3 ConvexPolyhedron::centerOnCentroid() {
	glm::vec3 centroid = getCentroid();
	for (glm::vec3& v : vertex) {
		v -= centroid;
	}
	return -centroid;
}

float ConvexPolyhedron::getVolume() {
	// Get a point on the inside
	glm::vec3 inner_point(0, 0, 0);
	for (const auto& v : vertex) {
		inner_point += v;
	}
	inner_point /= vertex.size();
	float volume = 0;
	for (const auto& f : face) {
		for (int k = 1; k < f.size() - 1; k++) {
			glm::vec3& a = vertex[f[0]];
			glm::vec3& b = vertex[f[k]];
			glm::vec3& c = vertex[f[k + 1]];
			glm::vec3& d = inner_point;
			volume += computeTetraVolume(a, b, c, d);
		}
	}
	return volume;
}

glm::mat3 ConvexPolyhedron::computeInertia(const float mass) {
	float total_volume = getVolume();
	// Get a point on the inside
	glm::vec3 inner_point(0, 0, 0);
	for (const auto& v : vertex) {
		inner_point += v;
	}
	inner_point /= vertex.size();
	glm::mat3 inertia = glm::mat3(0);
	float volume = 0;
	for (const auto& f : face) {
		for (int k = 1; k < f.size() - 1; k++) {
			glm::vec3& a = vertex[f[0]];
			glm::vec3& b = vertex[f[k]];
			glm::vec3& c = vertex[f[k + 1]];
			glm::vec3& d = inner_point;
			float vol = computeTetraVolume(a, b, c, d);
			inertia += computeTetraInertia(mass * vol / total_volume, a, b, c, d);
		}
	}
	return inertia;
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
	std::pair<glm::vec3, glm::vec3> AABB = { {FLT_MAX,FLT_MAX,FLT_MAX},{-FLT_MAX,-FLT_MAX,-FLT_MAX}} ;
	for (auto& v : vertex) {
		glm::vec3 wv = pose*glm::vec4(v,1.0f) ;
		AABB.first.x = fmin(AABB.first.x, wv.x);
		AABB.first.y = fmin(AABB.first.y, wv.y);
		AABB.first.z = fmin(AABB.first.z, wv.z);
		AABB.second.x = fmax(AABB.second.x, wv.x);
		AABB.second.y = fmax(AABB.second.y, wv.y);
		AABB.second.z = fmax(AABB.second.z, wv.z);
	}
	return AABB ;
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

// Returns an axis aligned bounding box
ConvexPolyhedron ConvexPolyhedron::makeAxisAlignedBox(glm::vec3 min, glm::vec3 max, float mass){
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
	return ConvexPolyhedron(vertices, faces, mass);
}

//Alternate form of box that always centers on the origin
ConvexPolyhedron ConvexPolyhedron::makeAxisAlignedBox(glm::vec3 size, float mass){
	return makeAxisAlignedBox(size * -0.5f, size * 0.5f, mass);
}

// Returns a shape for a cylinder with center of ends A and B and the given radius and side count
ConvexPolyhedron ConvexPolyhedron::makeCylinder(glm::vec3 A, glm::vec3 B, float radius, int sides, float mass){
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
	return ConvexPolyhedron(vertices, faces, mass);
}

// Returns a shape for a Tetrahedron with the given points
ConvexPolyhedron ConvexPolyhedron::makeTetra(glm::vec3 A, glm::vec3 B, glm::vec3 C, glm::vec3 D, float mass){
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
	return ConvexPolyhedron(vertices, faces, mass);
}


// Builds an approximate convex hull of the given model with up to the given number of faces
	// Detail level is sphere extrapolation used, it improves the quality but also increases the time taken exponentially
ConvexPolyhedron ConvexPolyhedron::makeApproximateHull(std::shared_ptr<GLTF>& model, float mass, int hull_faces, int detail_level){
	model->applyTransforms();
	std::vector<glm::dvec3> points ;
	for(auto& v : model->vertices){
		points.emplace_back(v.transformed_position) ;
	}
	std::vector<Polygon> poly = Polygon::buildApproximateHull(points,hull_faces,detail_level) ;
	if(mass <= 0 || mass > 1e10){
		return ConvexPolyhedron(poly);
	}else{
		return ConvexPolyhedron(poly,mass) ;
	}
}

//Same as above but first separates the mesh by connected closed surfaces
std::vector<ConvexPolyhedron> ConvexPolyhedron::makeApproximateSurfaceHulls(std::shared_ptr<GLTF>& model, float mass, int hull_faces, int detail_level) {
	model->applyTransforms();
	std::vector<ConvexPolyhedron> result;
	std::vector<std::vector<Polygon>> surfaces = Polygon::collectClosedSurfaces(model);
	for (auto& surface : surfaces) {
		std::vector<glm::dvec3> points;
		for(auto& poly : surface){
			for (auto& v : poly.p) {
				points.emplace_back(v);
			}
		}
		std::vector<Polygon> poly = Polygon::buildApproximateHull(points, hull_faces, detail_level);
		if (mass <= 0 || mass > 1e10) {
				result.emplace_back(poly);
		}
		else {
			result.emplace_back(poly, mass/surfaces.size()); // TODO distribute mass based on volume of pieces
		}
		
	}
	return result ;
}

//Collect the convex pieces of a model
std::vector<ConvexPolyhedron> ConvexPolyhedron::collectConvexPiecesByBSP(std::shared_ptr<GLTF>& model){
	std::vector<ConvexPolyhedron> result ;
	std::vector<std::vector<Polygon>> surfaces = Polygon::collectClosedSurfaces(model);

	printf("surfaces: %d\n", (int)surfaces.size()) ;
	for(auto& surface : surfaces){
		
		//std::unique_ptr<VolumeNode> root = std::make_unique<VolumeNode>(surface);
		//root->recurseToDepth(depth);
		std::unique_ptr<BSPNode> root = std::make_unique<BSPNode>(surface);
		
		auto hull_shapes = root->getHulls();
		
		
		for(auto& poly : hull_shapes){
			result.emplace_back(poly) ;
		}
	}

	return result ;
}


std::vector<ConvexPolyhedron> ConvexPolyhedron::collectConvexPiecesByRadialVolumes(std::shared_ptr<GLTF>& model, int depth){
	std::vector<ConvexPolyhedron> result;
	std::vector<std::vector<Polygon>> surfaces = Polygon::collectClosedSurfaces(model);

	printf("surfaces: %d\n", (int)surfaces.size());
	for (auto& surface : surfaces) {

		std::unique_ptr<VolumeNode> root = std::make_unique<VolumeNode>(surface);
		root->recurseToDepth(depth);
		auto hull_shapes = root->getHulls();


		for (auto& poly : hull_shapes) {
			result.emplace_back(poly);
		}
	}

	return result;
}

std::vector<ConvexPolyhedron> ConvexPolyhedron::collectConvexPiecesByBone(std::shared_ptr<GLTF>& model, int hull_faces, int detail_level, float min_weight, float min_bone_volume){
	std::map<int,std::vector<glm::dvec3>> bone_points;
	model->applyTransforms();
	for(auto& v : model->vertices){
		if(v.weights.x >= min_weight){
			bone_points[v.joints.x].emplace_back(v.transformed_position) ;
		}
		if (v.weights.y >= min_weight) {
			bone_points[v.joints.y].emplace_back(v.transformed_position);
		}
		if (v.weights.z >= min_weight) {
			bone_points[v.joints.z].emplace_back(v.transformed_position);
		}
		if (v.weights.w >= min_weight) {
			bone_points[v.joints.w].emplace_back(v.transformed_position);
		}
	}

	

	std::vector<int> to_combine ;
	for (auto& [bone, points] : bone_points) {
		glm::dvec3 min = glm::dvec3(FLT_MAX, FLT_MAX, FLT_MAX);
		glm::dvec3 max = glm::dvec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (auto& x : points) {
			min.x = fmin(min.x, x.x);
			min.y = fmin(min.y, x.y);
			min.z = fmin(min.z, x.z);
			max.x = fmax(max.x, x.x);
			max.y = fmax(max.y, x.y);
			max.z = fmax(max.z, x.z);
		}

		double volume = (max.x-min.x)*(max.y-min.y)*(max.z-min.z) ;
		if(volume < min_bone_volume){
			to_combine.push_back(bone) ;
		}
	}
	
	for(int k= 0 ; k < to_combine.size(); k++){ //TODO is this order guaranteed for roll up?
		int bone = to_combine[k] ;
		int parent = model->nodes[bone].parent ;
		printf("Bone %d merging to %d\n", bone, parent) ;
		if(parent >=0){
			for(auto& p : bone_points[bone]){
				bone_points[parent].push_back(p);
			}
		}
		printf("Erasing bone: %d\n", bone);
		bone_points.erase(bone) ;
	}


	std::vector<ConvexPolyhedron> result;
	for(auto& [bone, points] : bone_points){
		printf("Making bone: %d\n", bone) ;
		std::vector<Polygon> poly = Polygon::buildApproximateHull(points, hull_faces, detail_level);
		result.emplace_back(poly);
	}
	return result ;
}


int64_t Collision::getHash() const {
	return getHash(id1,shape1, id2, shape2, CONSTRAINT_TYPE);
}
bool Collision::updateConstraint(PhysicsContainer* cell){
	RigidBody* body_1 = cell->getBody(id1);
	RigidBody* body_2 = cell->getBody(id2);

	float velocity_against_normal = glm::dot(body_1->velocity - body_2->velocity, normal);

	float restitution_bias = 0.0f; // inelastic
	if (velocity_against_normal > min_velocity_for_elastic) {
		float e = fmax(body_1->elasticity,body_2->elasticity);
		restitution_bias = e * velocity_against_normal; // elastic
	}

	//Bias against penetration with spring force
	float penetration_bias = penetration_spring_coefficient * std::max(0.0f, penetration_depth - allowed_collision_depth);

	target = restitution_bias + penetration_bias;
	return true ; // TODO compute if still relevant
}
void Collision::applyWarmingImpulse(PhysicsContainer* cell){
	RigidBody* body_1 = cell->getBody(id1);
	RigidBody* body_2 = cell->getBody(id2);

	warm_tangent_impulse -= normal * glm::dot(normal,warm_tangent_impulse) ;
	warm_impulse = normal * glm::dot(normal, warm_impulse) ;

	tangents.clear();
	glm::vec3 ref = (std::abs(normal.y) < 0.8f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
	tangents.push_back(glm::normalize(glm::cross(normal, ref)));
	tangents.push_back(glm::normalize(glm::cross(normal, tangents[0])));

	glm::vec3 impulse = warm_impulse + warm_tangent_impulse ;

	body_1->velocity -= impulse * body_1->inv_mass;
	body_2->velocity += impulse * body_2->inv_mass;

	glm::vec3 r1 = point - body_1->position;
	glm::vec3 r2 = point - body_2->position;
	body_1->angular_velocity -= body_1->inv_moment * glm::cross(r1, impulse);// TODO inertia needs to be rotated based on pose of rigid body
	body_2->angular_velocity += body_2->inv_moment * glm::cross(r2, impulse);
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
	float rot_term1 = glm::dot(glm::cross(body_1->inv_moment * glm::cross(r1, normal), r1), normal); // TODO inertia needs to be rotated based on pose of rigid body
	float rot_term2 = glm::dot(glm::cross(body_2->inv_moment * glm::cross(r2, normal), r2), normal);
	float effective_mass_n = body_1->inv_mass + body_2->inv_mass + rot_term1 + rot_term2;
	if (effective_mass_n <= 1e-6f) {
		return; // two immovable objects
	}

	//Calculate current change needed based on already applied
	float impulse_mag_n = (target - velocity_along_normal) / effective_mass_n;
	float old_accumulated_n = glm::dot(warm_impulse, normal);
	float new_accumulated_n = std::max(0.0f, old_accumulated_n + impulse_mag_n);
	float current_impulse_n = new_accumulated_n - old_accumulated_n;
	glm::vec3 impulse_vec_n = normal * current_impulse_n;

	//Apply normal impulse
	body_1->velocity -= impulse_vec_n * body_1->inv_mass;
	body_2->velocity += impulse_vec_n * body_2->inv_mass;
	body_1->angular_velocity -= body_1->inv_moment * glm::cross(r1, impulse_vec_n);// TODO inertia needs to be rotated based on pose of rigid body
	body_2->angular_velocity += body_2->inv_moment * glm::cross(r2, impulse_vec_n);

	//update warm impulse
	warm_impulse += impulse_vec_n;

	// Recalculate velocities at point after normal impulse
	contact_velocity_1 = body_1->velocity + glm::cross(body_1->angular_velocity, r1);
	contact_velocity_2 = body_2->velocity + glm::cross(body_2->angular_velocity, r2);
	relative_velocity = contact_velocity_2 - contact_velocity_1;

	
	glm::vec3 tangent_impulse(0) ;
	for(auto& tangent : tangents){
		float velocity_along_tangent = glm::dot(tangent,relative_velocity); 

		// Effective mass for tangent direction
		float rot_term1_t = glm::dot(glm::cross(body_1->inv_moment * glm::cross(r1, tangent), r1), tangent);
		float rot_term2_t = glm::dot(glm::cross(body_2->inv_moment * glm::cross(r2, tangent), r2), tangent);
		float effective_mass_t = body_1->inv_mass + body_2->inv_mass + rot_term1_t + rot_term2_t;
		if(effective_mass_t < 1e-6f){
			continue ;
		}
		//Compute maximum tangent velocity ot be lost
		float impulse_mag_t = -1.0f * velocity_along_tangent / effective_mass_t;
		tangent_impulse += tangent * impulse_mag_t; ;		
	}

	glm::vec3 accumulated_friction = tangent_impulse + warm_tangent_impulse ;
	float friction_magnitude = glm::length(accumulated_friction);
	if(friction_magnitude < 1e-6f){
		accumulated_friction = glm::vec3(0,0,0);
	}else{
		float max_friction = (body_1->friction + body_2->friction) * 0.5f * new_accumulated_n;
		float clamped_magnitude = std::min(friction_magnitude, max_friction);
		accumulated_friction *= clamped_magnitude/friction_magnitude;
	}
	tangent_impulse = accumulated_friction - warm_tangent_impulse ;
	

	// Apply Tangent Impulse
	body_1->velocity -= tangent_impulse * body_1->inv_mass;
	body_2->velocity += tangent_impulse * body_2->inv_mass;
	body_1->angular_velocity -= body_1->inv_moment * glm::cross(r1, tangent_impulse);
	body_2->angular_velocity += body_2->inv_moment * glm::cross(r2, tangent_impulse);


	//update warm impulse
	warm_tangent_impulse = accumulated_friction;

}

//Retargets this constraint to the objects after it has moved
bool Collision::retargetConstraint(PhysicsContainer* cell){
	glm::vec3 a =cell->getBody(id1)->pose * glm::vec4(local_a,1) ;
	glm::vec3 b = cell->getBody(id2)->pose * glm::vec4(local_b, 1);
	glm::vec3 x = a-b ;
	float new_depth = glm::length(x);
	glm::vec3 new_normal = x/new_depth ;
	if(glm::dot(normal,new_normal) < retarget_normal_alignment_minimum){
		return false ;
	}
	point = (a+b)*0.5f ;
	penetration_depth = new_depth ;
	return true ;
}


//Returns an identifying hash that can be used to group constraints into this set
int64_t SinglePointCollision::getHash() const{
	return hashBytes(serialize(point.id1, point.id2, Collision::CONSTRAINT_TYPE));
}

//Add a constraint to this set
void SinglePointCollision::addConstraint(PhysicsContainer* cell, Constraint& new_constraint){
	Collision& new_point = static_cast<Collision&>(new_constraint) ;
	new_point.warm_impulse = point.warm_impulse;
	new_point.warm_tangent_impulse = point.warm_tangent_impulse ;
	point = new_point ;
}

//Update the constraint targets based on information at the start of the frame
//Returns if any of the constraints are active at all
bool SinglePointCollision::updateConstraints(PhysicsContainer* cell){
	return point.updateConstraint(cell);
	
}

//Apply starting impulses carried over if any constraint has existed for multiple frames in a row
void SinglePointCollision::applyWarmingImpulses(PhysicsContainer* cell){
	point.applyWarmingImpulse(cell);
}

//Applies impulses to velocity of involved bodies to satisfy these constraints
void SinglePointCollision::applyConstraints(PhysicsContainer* cell){
	point.applyConstraint(cell);
}

//Returns an identifying hash that can be used to group constraints into this set
int64_t ManifoldCollision::getHash() const {
	return hash ;
}

//Add a constraint to this set
void ManifoldCollision::addConstraint(PhysicsContainer* cell, Constraint& new_constraint) {
	Collision& new_point = static_cast<Collision&>(new_constraint);
	std::vector<int> to_keep;
	int closest = -1 ;
	float cd2 = FLT_MAX ;
	
	for(int k=0;k<points.size();k++){
		bool valid = points[k].retargetConstraint(cell);
		if(valid){
			to_keep.push_back(k);
			if (glm::distance2(points[k].point, new_point.point) < cd2) {
				closest = k ;
			}
		}
	}

	//Point is so close it's the same point
	if(closest >= 0 && cd2 < squared_distance_for_match){
		points[closest].local_a = new_point.local_a ;
		points[closest].local_b = new_point.local_b; // overwrite with new point 
		points[closest].point= new_point.point;
		points[closest].normal = new_point.normal; 
		// but carry over warm impulses
	}else if(closest >=0 && points.size() >= max_collision_points){ // Too many collision
		points[closest] = new_point ; // overwrite with new point
		// dont carry over warm impulses
	}else{ //We can have a totally new point
		to_keep.push_back((int)points.size()) ;
		points.push_back(new_point);
	}

	std::vector<Collision> new_points;
	for(int k : to_keep){
		new_points.push_back(points[k]) ;
	}
	points = new_points ;
	
}

//Update the constraint targets based on information at the start of the frame
//Returns if any of the constraints are active at all
bool ManifoldCollision::updateConstraints(PhysicsContainer* cell) {
	for (auto& p : points) {
		p.updateConstraint(cell);
	}
	return true ;
}

//Apply starting impulses carried over if any constraint has existed for multiple frames in a row
void ManifoldCollision::applyWarmingImpulses(PhysicsContainer* cell) {
	for(auto&p : points){
		p.applyWarmingImpulse(cell);
	}
}

//Applies impulses to velocity of involved bodies to satisfy these constraints
void ManifoldCollision::applyConstraints(PhysicsContainer* cell) {
	for(auto& p : points){
		p.applyConstraint(cell);
	}
}	


Sphere::Sphere(float r, float m){
	radius = r ;
	mass = m ;
	inv_mass = 1.0f/ m ;
	inv_moment = glm::mat3(1.0f/ ( 0.4f * m * r * r)) ;
	moment = glm::inverse(inv_moment); // TODO not invert
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
	glm::vec3 wp = pose * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	std::pair<glm::vec3, glm::vec3> AABB = { {wp.x-radius,wp.y-radius,wp.z-radius},{wp.x + radius,wp.y + radius,wp.z + radius} };
	return AABB;
}

bool AAABIntersect(const std::pair<glm::vec3, glm::vec3>& A, const std::pair<glm::vec3, glm::vec3>& B) {
	return A.first.x < B.second.x && B.first.x < A.second.x &&
		A.first.y < B.second.y && B.first.y < A.second.y &&
		A.first.z < B.second.z && B.first.z < A.second.z;
}


// return the volume of the given tetrahedron
float computeTetraVolume(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d) {
	return fabs(glm::dot(a - d, glm::cross(b - d, c - d))) / 6.0f;
}

// Returns the center of mass of the given tetrahedron
glm::vec3 computeTetraCentroid(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d) {
	return (a + b + c + d) / 4.0f;
}

// Returns the inertia tensor of the given tetrahedron about the origin
glm::mat3 computeTetraInertia(const float mass, const glm::vec3& A, const glm::vec3& B, const glm::vec3& C, const glm::vec3& D) {
	// pulled from "Explicit Exact Formulas for the 3D tetrahedron inertia tensor in terms of vertex coordinates"
	// by F. Tonon, Journal of Mathematics and Statistics

	const double x1 = A.x, y1 = A.y, z1 = A.z;
	const double x2 = B.x, y2 = B.y, z2 = B.z;
	const double x3 = C.x, y3 = C.y, z3 = C.z;
	const double x4 = D.x, y4 = D.y, z4 = D.z;
	double mu = 6.0 * mass;

	double x_group = x1 * x1 + x1 * x2 + x2 * x2 + x1 * x3 + x2 * x3 + x3 * x3 + x1 * x4 + x2 * x4 + x3 * x4 + x4 * x4;
	double y_group = y1 * y1 + y1 * y2 + y2 * y2 + y1 * y3 + y2 * y3 + y3 * y3 + y1 * y4 + y2 * y4 + y3 * y4 + y4 * y4;
	double z_group = z1 * z1 + z1 * z2 + z2 * z2 + z1 * z3 + z2 * z3 + z3 * z3 + z1 * z4 + z2 * z4 + z3 * z4 + z4 * z4;

	double a = mu * (y_group + z_group) / 60.0;
	double b = mu * (x_group + z_group) / 60.0;
	double c = mu * (x_group + y_group) / 60.0;

	double ap = mu * (2 * y1 * z1 + y2 * z1 + y3 * z1 + y4 * z1 + y1 * z2 + 2 * y2 * z2 + y3 * z2 + y4 * z2 + y1 * z3 + y2 * z3 + 2 * y3 * z3 + y4 * z3 + y1 * z4 + y2 * z4 + y3 * z4 + 2 * y4 * z4) / 120.0;
	double bp = mu * (2 * x1 * z1 + x2 * z1 + x3 * z1 + x4 * z1 + x1 * z2 + 2 * x2 * z2 + x3 * z2 + x4 * z2 + x1 * z3 + x2 * z3 + 2 * x3 * z3 + x4 * z3 + x1 * z4 + x2 * z4 + x3 * z4 + 2 * x4 * z4) / 120.0;
	double cp = mu * (2 * x1 * y1 + x2 * y1 + x3 * y1 + x4 * y1 + x1 * y2 + 2 * x2 * y2 + x3 * y2 + x4 * y2 + x1 * y3 + x2 * y3 + 2 * x3 * y3 + x4 * y3 + x1 * y4 + x2 * y4 + x3 * y4 + 2 * x4 * y4) / 120.0;

	glm::mat3 J;
	J[0][0] = (float)a;
	J[0][1] = (float)-bp;
	J[0][2] = (float)-cp;
	J[1][0] = (float)-bp;
	J[1][1] = (float)b;
	J[1][2] = (float)-ap;
	J[2][0] = (float)-cp;
	J[2][1] = (float)-ap;
	J[2][2] = (float)c;

	return J;
}

//Find the support point of the minkowski difference of two shapes
//Saves the points on the shapes for later reconstruction
SupportPoint findSupportPoint(const glm::vec3 direction, const RigidBody* A, const int shapeA, const RigidBody* B, const int shapeB) {
	SupportPoint sp;
	sp.a = A->pose * glm::vec4( A->shape[shapeA]->support(A->inv_pose* glm::vec4(direction,0)), 1);
	sp.b = B->pose * glm::vec4(B->shape[shapeB]->support(B->inv_pose * glm::vec4(-direction, 0)), 1);
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
std::vector<SupportTriangle> detectCollision(const RigidBody* A, int shapeA, const RigidBody* B, int shapeB, int max_iterations) {
	// arbitrary first direction
	glm::vec3 search_direction = glm::vec3(1, 0, 0);
	const glm::vec3 origin(0, 0, 0);
	//std::vector<SupportPoint> p;
	SupportPoint p0 = findSupportPoint(search_direction, A,shapeA, B, shapeB);
	search_direction = origin - p0.x; // From p0 to origin
	SupportPoint p1 = findSupportPoint(search_direction, A, shapeA, B, shapeB);
	//New point could not get past zero in search direction
	if (glm::dot(p1.x, search_direction) <= 0) {
		return {}; // No collision
	}
	//Search perpendicular to p0 to p1 segment, toward origin
	search_direction = glm::cross(cross(p1.x - p0.x, search_direction), p1.x - p0.x);
	SupportPoint p2 = findSupportPoint(search_direction, A, shapeA, B, shapeB);
	if (glm::dot(p2.x, search_direction) <= 0) {
		return {}; // No collision
	}

	SupportTriangle first_triangle(p0, p1, p2);
	//Search along normal of triangle toward origin
	if (first_triangle.d < 0) { // facing wrong way to start
		first_triangle = SupportTriangle(p1, p0, p2); // flip winding order
	}

	search_direction = first_triangle.normal;
	SupportPoint p3 = findSupportPoint(search_direction, A, shapeA, B, shapeB);
	if (glm::dot(p3.x, search_direction) <= 0) {
		return {}; // No collision
	}

	std::vector<SupportTriangle> simplex = buildSupportSimplex(first_triangle, p3);

	for (int iter = 0; iter < max_iterations; iter++) {
		bool found_triangle = false;
		for (int k = 1; k < 4; k++) { // First triangle always what simpex was built from andwill always face out
			if (simplex[k].signedDistance(origin) > 0) {
				SupportPoint new_point = findSupportPoint(simplex[k].normal, A, shapeA, B, shapeB);
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
SupportPoint getPenetration(std::vector<SupportTriangle>& collision_result, const RigidBody* A, int shapeA, const RigidBody* B, int shapeB, int max_iterations) {
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
		SupportPoint new_point = findSupportPoint(active_face.normal, A, shapeA, B, shapeB);

		float signed_distance = active_face.signedDistance(new_point.x);
		//printf("D:%f\n", selected_distance) ;
		//Expansion didn't expand means we've reached closest surface face
		if (signed_distance < 1e-4f || iterations == max_iterations) {
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

SimpleLocalPhysicsCell::SimpleLocalPhysicsCell() {}

//Custom destructor cleans up scene instance
SimpleLocalPhysicsCell::~SimpleLocalPhysicsCell() {
	ScenePlugin* scene = getTool<ScenePlugin>();
	for (auto& [id, type_sceneid] : instance) {
		scene->deleteInstance(type_sceneid.second);
	}
}

int SimpleLocalPhysicsCell::addType(std::shared_ptr<Physics::ConvexShape> shape, const std::string& model, glm::mat4& transform, float elasticity, float friction) {
	int id = next_type_id;
	next_type_id++;
	types[id] = { {shape}, model, transform, elasticity, friction };
	return id;
}


int SimpleLocalPhysicsCell::addType(std::vector<std::shared_ptr<Physics::ConvexShape>> shape, const std::string& model, glm::mat4& transform, float elasticity, float friction) {
	int id = next_type_id;
	next_type_id++;
	types[id] = { shape, model, transform, elasticity, friction };
	return id;
}

int SimpleLocalPhysicsCell::addType(std::vector<Physics::ConvexPolyhedron> raw_shape, const std::string& model, glm::mat4& transform, float elasticity, float friction) {
	std::vector<std::shared_ptr<ConvexShape>> shape ;
	for(auto& s : raw_shape){
		std::shared_ptr<Physics::ConvexPolyhedron> sh= std::make_shared<Physics::ConvexPolyhedron>(s, transform, s.mass);
		shape.push_back(sh) ;
	}
	return addType(shape, model, transform, elasticity, friction) ;
}

int64_t SimpleLocalPhysicsCell::add(int type, const glm::vec3& pos, const glm::vec3& vel, const glm::vec3& a_vel) {
	int64_t id = next_object_id++;
	ScenePlugin* scene = getTool<ScenePlugin>();
	instance[id] = { type, scene->createInstance(types[type].model, glm::mat4(0)) };
	bodies[id] = std::make_shared<Physics::RigidBody>(types[type].shape, id, pos, vel, a_vel);
	bodies[id]->elasticity = types[type].elasticity;
	bodies[id]->friction = types[type].friction;
	return id;
}

Physics::RigidBody* SimpleLocalPhysicsCell::getBody(int64_t id) {
	auto iter = bodies.find(id);
	if (iter != bodies.end()) {
		return iter->second.get();
	}
	else {
		return nullptr;
	}
}

//Consraint id should be a hash of the involved bodies and the type of constraint
Physics::ConstraintSet* SimpleLocalPhysicsCell::getConstraintSet(int64_t id) {
	auto iter = constraints.find(id);
	if (iter != constraints.end()) {
		return iter->second.get();
	}
	else {
		return nullptr;
	}
}

//Finds all collisions of the balls with each other and the walls of the cell
//Creates or destroys constraints so the contents of constraints matches the current collisions
//Also sets points and normal for collisions
void SimpleLocalPhysicsCell::updateCollisions() {
	std::unordered_set<int64_t> found_constraints;
	for (auto& [id1, body_1] : bodies) {
		//Ball to ball collisions
		for (auto& [id2, body_2] : bodies) {
			if (id1 < id2 && // only check each pair once
				(body_1->inv_mass > 0 || body_2->inv_mass > 0) && // only check if one is moveable
				Physics::AAABIntersect(body_1->AABB, body_2->AABB)) { // check AABBs first

				for(int shapeA = 0; shapeA < body_1->shape.size(); shapeA++){
					for (int shapeB = 0; shapeB < body_2->shape.size(); shapeB++) {

						auto simplex = Physics::detectCollision(body_1.get(), shapeA, body_2.get(), shapeB);
						if (simplex.size() > 0) {
							Physics::SupportPoint sp = Physics::getPenetration(simplex, body_1.get(),shapeA,body_2.get(), shapeB);
							if (glm::length(sp.x) > Physics::Collision::allowed_collision_depth * 0.5f) {
								glm::vec3 point = (sp.a + sp.b) * 0.5f;
								glm::vec3 normal = glm::normalize(sp.x);

								normal = glm::normalize(normal);
								int64_t constraint_id = Collision::getHash(id1, shapeA, id2, shapeB, Physics::Collision::CONSTRAINT_TYPE);
								found_constraints.insert(constraint_id); // track found so we can remove not found
						
								std::shared_ptr<Physics::Collision> constraint = std::make_shared<Physics::Collision>();
								constraint->id1 = id1;
								constraint->shape1 = shapeA;
								constraint->id2 = id2;
								constraint->shape2 = shapeB ;
								constraint->point = point;
								constraint->normal = normal;
								constraint->local_a = body_1->inv_pose * glm::vec4(sp.a,1) ;
								constraint->local_b = body_2->inv_pose * glm::vec4(sp.b, 1) ;
								constraint->penetration_depth = glm::length(sp.x) ;

								if(constraints.find(constraint_id) == constraints.end()){
									constraints[constraint_id] = std::make_shared<ManifoldCollision>(constraint_id) ;
									//constraints[constraint_id] = std::make_shared<SinglePointCollision>();
								}

								constraints[constraint_id]->addConstraint(this, *constraint.get()) ;
						
							}
						}
					}
				}
			}
		}

	}

	//Delete existing constraints not found now
	std::vector<int64_t> to_delete;
	for (auto& [id, constraint] : constraints) {
		if (found_constraints.find(id) == found_constraints.end()) {
			to_delete.push_back(id);
		}
	}
	for (auto& id : to_delete) {
		constraints.erase(id);
	}
}

//Run physics forward one frame
void SimpleLocalPhysicsCell::runPhysicsFrame(float dt, int constraints_iter) {
	for (auto& [id, body] : bodies) {
		body->integrateAcceleration(acceleration, dt);
	}
	for (auto& [id, constraint] : constraints) {
		constraint->updateConstraints(this);
	}
	for (auto& [id, constraint] : constraints) {
		constraint->applyWarmingImpulses(this);
	}
	for (int i = 0; i < constraints_iter; i++) {
		for (auto& [id, constraint] : constraints) {
			constraint->applyConstraints(this);
		}
	}
	for (auto& [id, ball] : bodies) {
		ball->integrateVelocity(dt);
	}
	updateCollisions();
}

//Uses types to render all objects with the scene plugin
void SimpleLocalPhysicsCell::updateGraphics() {
	ScenePlugin* scene = getTool<ScenePlugin>();
	for (auto& [id, ball] : bodies) {
		if (id > 0) { // it's a ball
			auto iter = instance.find(id);
			glm::mat4 pose = glm::mat4(1.0f);
			pose = glm::translate(pose, ball->position);
			pose = pose * glm::mat4_cast(ball->orientation);
			pose = pose * types[iter->second.first].render_transform;
			scene->setPose(instance[id].second, pose);
		}
	}
}


//Sets the pose of an object
void SimpleLocalPhysicsCell::setPose(int64_t id, const glm::mat4& pose){
	auto iter = bodies.find(id);
	if(iter != bodies.end()){
		printf("Set pose\n");
		iter->second->setPose(pose) ;
	}

}

} // end namespace Physics