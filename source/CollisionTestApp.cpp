#include "CollisionTestApp.h"
#include "ParticlePlugin.h"
#include "ScenePlugin.h"
#include "FlagSet.h"


CollisionTestApp::ConvexPolyhedron::ConvexPolyhedron(const std::vector<glm::vec3>& vertices, const std::vector<std::vector<int>>& faces) {
	vertex = vertices;
	face = faces;
}

CollisionTestApp::ConvexPolyhedron::ConvexPolyhedron(std::shared_ptr<ConvexPolyhedron> base, const glm::mat4& pose){
	vertex = base->vertex ;
	face = base->face;
	for(auto& v : vertex){
		v = pose * glm::vec4(v,1.0f) ;
	}
}

// Returns a shape for an axis aligned bounding box
CollisionTestApp::ConvexPolyhedron CollisionTestApp::ConvexPolyhedron::makeAxisAlignedBox(glm::vec3 min, glm::vec3 max) {
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
CollisionTestApp::ConvexPolyhedron CollisionTestApp::ConvexPolyhedron::makeAxisAlignedBox(glm::vec3 size) {
	return makeAxisAlignedBox(size * -0.5f, size * 0.5f);
}

glm::vec3 CollisionTestApp::ConvexPolyhedron::support(const glm::vec3& direction){
	float highest = -FLT_MAX ;
	glm::vec3 support ;
	//TODO walk edge graph to make this more efficient for more complex polyhedron
	for(auto& v : vertex){
		float dot = glm::dot(v,direction);
		if(dot > highest){
			highest = dot ;
			support = v ;
		}
	}
	return support ;
}

// Returns a shapefor a cylinder with center of ends and A and B
CollisionTestApp::ConvexPolyhedron CollisionTestApp::ConvexPolyhedron::makeCylinder(glm::vec3 A, glm::vec3 B, float radius, int sides) {
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
CollisionTestApp::ConvexPolyhedron CollisionTestApp::ConvexPolyhedron::makeTetra(glm::vec3 A, glm::vec3 B, glm::vec3 C, glm::vec3 D) {
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


CollisionTestApp::PolyInstance::PolyInstance(std::string model, std::shared_ptr<ConvexPolyhedron> base) {
	base_shape = base;
	pose = glm::mat4(1.0f);
	scene_id = getTool<ScenePlugin>()->createInstance(model, pose);
}

void CollisionTestApp::PolyInstance::setPose(glm::mat4& p){
	pose = p ;
	getTool<ScenePlugin>()->setPose(scene_id,pose) ;
	world_shape = std::make_shared<ConvexPolyhedron>(base_shape, pose) ;
}

CollisionTestApp::CollisionTestApp() {}

// Called when switching into this state before the first time run is called
void CollisionTestApp::enter(std::shared_ptr<MachineState> from) {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	//Load the model
	//scene->createModelSet(Ball::BALL_MODEL, Ball::BALL_MODEL, true);

	// Make a particle for the mouse
	mouse_particle_id = particles->createParticle(0);
	particles->setColor(mouse_particle_id, glm::vec4(1, 0, 0, 1));
	glm::mat4 particle_pose = glm::mat4(1.0f);
	particles->setPose(mouse_particle_id, particle_pose);

	// Set up a light for the scene
	ScenePlugin::LightComponent lc;
	glm::vec3 light_position = glm::vec3(15, 15, -5);
	glm::vec3 look_at = glm::vec3(0, 0, 0);
	lc.light_color = glm::vec4(0.5, 0.5, 0.5, 1);
	light_id = scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_position, look_at, glm::vec3(0, 1, 0), 0.55f, 30, 2048, 0, lc);

	// Place the camera
	glm::vec3 camera_position = { 0,20,-3 };
	float fov = 0.7f;
	window->window_target->setCamera(camera_position, look_at, fov, glm::vec3(0, 1, 0));


	base_shape["box"] = std::make_shared<ConvexPolyhedron>(ConvexPolyhedron::makeAxisAlignedBox(glm::vec3(0.1,0.1,0.1)));
	base_shape["tetra"] = std::make_shared<ConvexPolyhedron>(ConvexPolyhedron::makeTetra(glm::vec3(0, 0, 0.1), glm::vec3(0.1, 0, 0),glm::vec3(0, 0.1, 0),glm::vec3(0, 0, 0))) ;
	base_shape["cylinder"] = std::make_shared<ConvexPolyhedron>(ConvexPolyhedron::makeCylinder(glm::vec3(0, 0, 0.1), glm::vec3(0, 0, -0.1), 0.05f, 8)) ;

	int c = 0 ;
	for(auto& [name, base] : base_shape){
		std::shared_ptr<GLTF> model = std::make_shared<GLTF>();
		model->setPolyhedronModel(base->vertex,base->face,colors[c]) ;
		scene->createModelSet(name,model,false, true) ;
		instances.emplace_back(name,base);
		glm::mat4 pose = glm::translate(glm::mat4(1.0f), positions[c]) ;
		instances[instances.size()-1].setPose(pose) ;
		c++;
	}


	std::shared_ptr<GLTF> box = std::make_shared<GLTF>();
	box->setBoundingBoxModel(min, max, glm::vec4(1, 1, 1, 1));
	box = box->createMirrorImage(); // Flips winding order inside out
	scene->createModelSet("room", box);
	room_instance_id = scene->createInstance("room", glm::mat4(1.0f));


	if(show_grid){
		glm::vec3 pmin = min*0.5f;
		glm::vec3 pmax = max*0.5f;
		float step = 0.25f ;
		for(float x = pmin.x ; x <= pmax.x; x+= step){
		for (float y = pmin.y; y <= pmax.y; y += step) {
		for (float z = pmin.z; z <= pmax.z; z += step) {
			std::shared_ptr<Point> p = std::make_shared<Point>(glm::vec3(x + randomFloat() * 0.0001f,y+randomFloat()*0.0001f,z + randomFloat() * 0.0001f)) ;
			p->particle_id = particles->createParticle(0);
			glm::mat4 particle_pose = glm::mat4(1.0f);
			particle_pose = glm::translate(particle_pose, p->x);
			particle_pose = glm::scale(particle_pose, glm::vec3(particle_size, particle_size, particle_size));
			particles->setPose(p->particle_id, particle_pose);
			particles->setColor(p->particle_id, glm::vec4(0,0,0,1)) ;
			points.emplace_back(p);
		}}}
	}
}

//Called every frame while the state is active
void CollisionTestApp::run() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	// Get the current time and time slice of the frame
	current_time = now();
	float dt = microsBetween(last_run_time, current_time) / 1000000.0f;
	if (dt < 0 || dt > 0.5f) {
		dt = 0; // don't move on frames where something is amiss with the clock
	}

	last_run_time = current_time;

	// get the 3D ray from the mouse position on the screen
	glm::vec3 ray_origin = window->window_target->camera_position;
	glm::vec3 ray_direction = window->getMouseRay();

	float t = -1; // TODO implement raytracing to make balls clickable
	//Place the mouse particle
	glm::vec3 mouse_position;
	if (t > 0) { // collision
		particles->setColor(mouse_particle_id, glm::vec4(0, 0, 1, 1)); // blue
		mouse_position = window->window_target->camera_position + window->getMouseRay() * t; // hit postion
	}
	else { // no collision
		particles->setColor(mouse_particle_id, glm::vec4(1, 0, 0, 1)); // red
		mouse_position = window->window_target->camera_position + window->getMouseRay() * 3.0f; // arbitrary depth on no collision
	}
	glm::mat4 particle_pose = glm::mat4(1.0f);
	particle_pose = glm::translate(particle_pose, mouse_position);
	particle_pose = glm::scale(particle_pose, glm::vec3(particle_size, particle_size, particle_size));
	particles->setPose(mouse_particle_id, particle_pose);


	int c = 0 ;
	if(! window->keyDown(SDLK_SPACE) && !OpenXRPlugin::ENABLED){
		for(auto& inst : instances){
			glm::mat4 pose = glm::translate(glm::mat4(1.0f), positions[c]);
			pose = glm::rotate(pose,timeMilliseconds()/1000.0f, glm::vec3(0,1,0)) ;
			inst.setPose(pose);
			c++;
		}
	}

	if(OpenXRPlugin::ENABLED){
		OpenXRPlugin* controls = getTool<OpenXRPlugin>();
		glm::mat4 current_left_hand_pose = controls->getPose("/actions/general/in/left_pose");
		glm::mat4 current_right_hand_pose = controls->getPose("/actions/general/in/right_pose");
		int c=0 ;
		for (auto& inst : instances) {
			glm::mat4 hand_pose ;
			if(c == 0){
				hand_pose = current_left_hand_pose ;
			}else if(c == 1){
				hand_pose = current_right_hand_pose;
			}else{
				break ;
			}

			inst.setPose(hand_pose);
			c++;
		}
	}


	for(auto& point : points){
		bool hit = false;
		for (auto& inst : instances) {
			hit |= detectCollision(inst.world_shape, point).size() != 0 ;
		}
		glm::mat4 particle_pose = glm::mat4(1.0f);
		particle_pose = glm::translate(particle_pose, point->x);
		if(hit){
			particle_pose = glm::scale(particle_pose, glm::vec3(particle_size, particle_size, particle_size));
		}else{
			particle_pose = glm::scale(particle_pose, glm::vec3(particle_size*0.3f, particle_size * 0.3f, particle_size * 0.3f));
		}
		particles->setPose(point->particle_id, particle_pose);
	}



	for(auto& id : last_display_particles){
		particles->destroyParticle(id);
	}
	last_display_particles = display_particles ;
	display_particles.clear();


	for(int k=1;k<instances.size();k++){
		for(int j=0;j<k;j++){
			auto result = detectCollision(instances[k].world_shape, instances[j].world_shape);
			if(result.size() > 0){
				/*
				glm::vec3 O(0,0,0) ;
				glm::vec3 A  = result[0].A.x ;
				glm::vec3 AB = result[0].B.x - result[0].A.x;
				glm::vec3 AC = result[0].C.x - result[0].A.x;
				glm::vec3 AD = result[1].A.x - result[0].A.x;
				glm::mat3 M(AB,AC,AD) ;
				M = glm::inverse(M) ;
				glm::vec3 bcd = M * (O-A) ;
				float b = bcd.x ;
				float c = bcd.y ;
				float d = bcd.z ;
				float a = 1 - b - c -d ;

				glm::vec3 check = a * A + b * result[0].B.x + c * result[0].C.x + d * result[1].A.x;
				printf("Check: %f, %f ,%f \n", check.x, check.y, check.z);
				glm::vec3 p1 = a * result[0].A.a + b * result[0].B.a + c * result[0].C.a + d * result[1].A.a ;
				glm::vec3 p2 = a * result[0].A.b + b * result[0].B.b + c * result[0].C.b + d * result[1].A.b ;

				glm::vec3 p = (p1 + p2)*0.5f ;
				printf("Found collision: %f, %f ,%f == %f,%f,%f\n", p1.x, p1.y, p1.z, p2.x,p2.y,p2.z);
				*/

				SupportPoint collision = getPenetration(result, instances[k].world_shape, instances[j].world_shape) ;

				int p_id = particles->createParticle(0);
				glm::mat4 particle_pose = glm::mat4(1.0f);
				particle_pose = glm::translate(particle_pose, collision.a);
				particle_pose = glm::scale(particle_pose, glm::vec3(particle_size, particle_size, particle_size));
				particles->setPose(p_id, particle_pose);
				particles->setColor(p_id, glm::vec4(0, 0, 0, 1));
				display_particles.push_back(p_id);

				p_id = particles->createParticle(0);
				particle_pose = glm::mat4(1.0f);
				particle_pose = glm::translate(particle_pose, collision.b);
				particle_pose = glm::scale(particle_pose, glm::vec3(particle_size, particle_size, particle_size));
				particles->setPose(p_id, particle_pose);
				particles->setColor(p_id, glm::vec4(0, 0, 0, 1));
				display_particles.push_back(p_id);
			}

		}
	}


	updateCamera();

	// Check if escape pressed to exit
	if (window->getLastKeyPress() == SDLK_ESCAPE) {
		getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
	}
}

// Called when switching out of this state after the last time run is called
void CollisionTestApp::exit(std::shared_ptr<MachineState> to) {
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	particles->destroyParticle(mouse_particle_id);
}

void CollisionTestApp::updateCamera() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	if (window->mouseDown(3)) { // right mouse button
		if (!mouse_down_right) {
			mouse_down_position_right = window->getMousePosition();
			camera_down_thi = camera_thi;
			camera_down_theta = camera_theta;
		}
		glm::vec2 mouse_position = window->getMousePosition();
		mouse_down_right = true;
		camera_theta = camera_down_theta + camera_x_speed * (mouse_position.x - mouse_down_position_right.x);
		camera_thi = camera_down_thi + camera_y_speed * (mouse_position.y - mouse_down_position_right.y);
		camera_thi = fmax(fmin(camera_thi, 3.14159f * 0.5f), 0.0f);
		mouse_down_position_right = window->getMousePosition();
		camera_down_thi = camera_thi;
		camera_down_theta = camera_theta;

	}
	else {
		mouse_down_right = false;
	}

	if (mouse_wheel_y_previous < window->getMouseWheelPosition().y) {
		zoom *= 0.95f;
	}
	else if (mouse_wheel_y_previous > window->getMouseWheelPosition().y) {
		zoom /= 0.95f;
	}

	if (zoom < 0.05f) {
		zoom = 0.05f ;
	}
	mouse_wheel_y_previous = window->getMouseWheelPosition().y;

	glm::vec3 camera_position = glm::vec3(cosf(camera_theta) * cosf(camera_thi), sinf(camera_thi), sinf(camera_theta) * cosf(camera_thi)) * zoom;
	window->window_target->setCamera(camera_position, look_at, fov, glm::vec3(0, 1, 0));

	glm::vec3 light_position = glm::vec3(cosf(light_theta) * cosf(light_thi), sinf(light_thi), sinf(light_theta) * cosf(light_thi)) * light_zoom;

	scene->moveLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_id, light_position, light_look_at, glm::vec3(0, 1, 0), light_fov, 30);
}


//Find the support point of the minkowski difference of two shapes
//Saves the points on the shapes for later reconstruction
CollisionTestApp::SupportPoint CollisionTestApp::findSupportPoint(const glm::vec3 direction, const std::shared_ptr<CollisionShape>& A, const std::shared_ptr<CollisionShape>& B){
	SupportPoint sp ;
	sp.a = A->support(direction);
	sp.b = B->support(-direction) ;
	sp.x = sp.a - sp.b ;
	return sp ;
}


//Build a support simplex from a triangle facing a point
std::vector<CollisionTestApp::SupportTriangle> buildSupportSimplex(const CollisionTestApp::SupportTriangle& triangle, const CollisionTestApp::SupportPoint& D){
	std::vector<CollisionTestApp::SupportTriangle> simplex ; 
	simplex.emplace_back(triangle.B, triangle.A, triangle.C) ; // flip initial triangle as outside is now inside
	simplex.emplace_back(D, triangle.B, triangle.C);
	simplex.emplace_back(triangle.A, D, triangle.C); // New triangles incorporating point and facing outward
	simplex.emplace_back(triangle.A, triangle.B, D);
	return simplex ;
}

//Uses GJK to detect whether two convex shapes collide
//If they collide this returns a simplex in Minkowski diference space enclosing the collision point
//If they do not collide, this returns an empty vector
std::vector<CollisionTestApp::SupportTriangle> CollisionTestApp::detectCollision(const std::shared_ptr<CollisionShape>& A, const std::shared_ptr<CollisionShape>& B){
	// arbitrary first direction
	glm::vec3 search_direction = glm::vec3(1, 0, 0);
	const glm::vec3 origin(0,0,0) ;
	//std::vector<SupportPoint> p;
	SupportPoint p0 = findSupportPoint(search_direction, A , B) ;
	search_direction = origin-p0.x ; // From p0 to origin
	SupportPoint p1 = findSupportPoint(search_direction, A, B);
	//New point could not get past zero in search direction
	if (glm::dot(p1.x, search_direction) <= 0) {
		return {}; // No collision
	}
	//Search perpendicular to p0 to p1 segment, toward origin
	search_direction = glm::cross(cross(p1.x- p0.x,search_direction), p1.x - p0.x) ;
	SupportPoint p2 = findSupportPoint(search_direction, A, B) ;
	if (glm::dot(p2.x, search_direction) <= 0) {
		return {}; // No collision
	}

	SupportTriangle first_triangle(p0, p1,p2) ;
	//Search along normal of triangle toward origin
	if(first_triangle.signedDistance(origin) < 0){ // facing wrong way to start
		first_triangle = SupportTriangle(p1, p0, p2); // flip winding order
	}

	search_direction = first_triangle.normal ;
	SupportPoint p3 = findSupportPoint(search_direction, A, B);
	if (glm::dot(p3.x, search_direction) <= 0) {
		return {}; // No collision
	}
	
	std::vector<CollisionTestApp::SupportTriangle> simplex = buildSupportSimplex(first_triangle, p3);

	for(int iter = 0; iter < MAX_GJK_ITERATIONS; iter++){
		bool found_triangle = false;
		for(int k=0; k < 4; k++){
			if(simplex[k].signedDistance(origin) > 0){
				SupportPoint new_point = findSupportPoint(simplex[k].normal, A, B);
				//New point could not get past zero in search direction
				if (glm::dot(new_point.x, simplex[k].normal) <= 0) {
					return {}; // No collision
				}
				simplex = buildSupportSimplex(simplex[k], new_point) ;
				found_triangle = true ;
				break ;
			}
		}
		if(!found_triangle){ // origin was insdide all faces
			return simplex ; // collision detected
		}
	}
	return {} ;

}

void CollisionTestApp::countEdge(const SupportPoint& A, const SupportPoint& B, std::unordered_map<SupportEdge, int>& edge_counts){
	SupportEdge edge = {A,B} ;
	auto iter = edge_counts.find(edge);
	if(iter == edge_counts.end()){
		edge_counts[edge] = 1 ;
	}else{
		iter->second++;
	}
}

//Uses expanding polytope algorithm on result of detectCollision
// Returns a supportPoint containg the resoltuion vector in x and the closets points on the shapes in a and b
CollisionTestApp::SupportPoint CollisionTestApp::getPenetration(std::vector<SupportTriangle>& collision_result,const std::shared_ptr<CollisionShape>& A, const std::shared_ptr<CollisionShape>& B){
	std::vector<SupportTriangle> polytope = collision_result;
	std::unordered_map<SupportEdge, int> edge_counts ;
	std::vector<SupportTriangle>new_polytope;

	int iterations = 0 ;
	while(true){
		
		//Find the nearest triangle on the polytope to the origin
		int selected_triangle = -1 ;
		float selected_distance = FLT_MAX;
		for(int k=0;k<polytope.size();k++){
			if(fabs(polytope[k].d) < selected_distance){
				selected_triangle = k ;
				selected_distance = fabs(polytope[k].d) ;
			}
		}
		SupportTriangle& active_face = polytope[selected_triangle] ;
		//use it's normal to expand to a new point
		SupportPoint new_point = findSupportPoint(active_face.normal,A, B) ;

		//Expansion didn't expand means we've reached closest surface face
		if(active_face.signedDistance(new_point.x) < 0.003f || iterations == MAX_GJK_ITERATIONS){
			glm::vec3 closest_x = active_face.normal * (-active_face.d) ; // closest point in minkowski space on plane
			//Get barycentric coordinates via area method
			glm::vec3 v0 = closest_x - active_face.A.x;
			glm::vec3 v1 = closest_x - active_face.B.x;
			glm::vec3 v2 = closest_x - active_face.C.x;
			float area_tot = glm::length(glm::cross(active_face.B.x - active_face.A.x, active_face.C.x - active_face.A.x));
			float a = glm::length(glm::cross(v1, v2)) / area_tot;
			float b = glm::length(glm::cross(v2, v0)) / area_tot;
			float c = 1.0f - a - b;

			SupportPoint collision_point = active_face.A * a + active_face.B * b + active_face.C * c ;
			//printf("a:%f, b:%f, c:%f\n", a,b,c) ;
			//printf(" %f == %f, %f == %f, %f == %f\n",collision_point.x.x, closest_x.x, collision_point.x.y, closest_x.y, collision_point.x.z, closest_x.z) ;
			return collision_point ;
		}

		//Remove all triangles facing the new point and collect their edges
		for(int k=0;k<polytope.size(); k++){
			if(polytope[k].signedDistance(new_point.x) > 0){
				countEdge(polytope[k].A, polytope[k].B, edge_counts) ;
				countEdge(polytope[k].B, polytope[k].C, edge_counts) ; 
				countEdge(polytope[k].C, polytope[k].A, edge_counts) ;// Order of points matters here to make sure new triangles face outward
			}else{
				// triangles not facing new point are carried into new polytope
				new_polytope.push_back(polytope[k]) ; 
			}
		}
		//Add new triangles made from edges of the removed triangles and the new point
		for(auto& [edge, count] : edge_counts){
			if(count == 1){ // outer edge of removed surface
				new_polytope.emplace_back(edge.A, edge.B, new_point) ;
			}
		}
		polytope = new_polytope ;
		new_polytope.clear();
		edge_counts.clear();
		iterations++;
	}
	
}