#include "NetPhysicsApp.h"
#include "ParticlePlugin.h"
#include "ScenePlugin.h"
#include "FlagSet.h"

NetPhysicsApp::NetPhysicsApp() {}

// Called when switching into this state before the first time run is called
void NetPhysicsApp::enter(std::shared_ptr<MachineState> from) {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>();

	NetPhysics::registerPhysics();

	// Set up a light for the scene
	ScenePlugin::LightComponent lc;
	glm::vec3 light_position = glm::vec3(15, 0.5, -0.5);
	glm::vec3 look_at = glm::vec3(0, 0, 0);
	lc.light_color = glm::vec4(0.5, 0.5, 0.5, 1);
	light_id = scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_position, look_at, glm::vec3(0, 1, 0), 0.55f, 30, 2048, 0, lc);

	// Place the camera
	glm::vec3 camera_position = { 0,20,-3 };
	float fov = 0.7f;
	window->window_target->setCamera(camera_position, look_at, fov, glm::vec3(0, 1, 0));


	createViewTypes();
	//printf("Attempting to connect to existing simulation...\n");
	worlds->connect("127.0.0.1", port, version);
	//host();
	//connect_time = now();
	
	
	/*
	transform = glm::scale(glm::mat4(1.0f), glm::vec3(mouse_size, mouse_size, mouse_size));
	std::shared_ptr<NetPhysics::Sphere> mouse_shape = std::make_shared<NetPhysics::Sphere>(mouse_size);
	int mouse_type = cell->addType(mouse_shape, BALL_MODEL, transform, 0.0f, 0.0f);
	mouse_body = cell->add(mouse_type, mid);

	for (auto& [id, body] : cell->bodies) {
		cell->disableCollision(id, mouse_body);
	}
	*/
}

//Called every frame while the state is active
void NetPhysicsApp::run() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>();


	if(!worlds->amHosting() && !worlds->connected()){
		if(!worlds->connectionPending()){
			printf("Connection attempt timed out. Hosting...\n");
			host();
		}
		return ;
	}
	

	// Get the current time and time slice of the frame
	current_time = now();
	float dt = microsBetween(last_run_time, current_time) / 1000000.0f;

	if (dt <= 0.001f || dt > 0.1f) {
		dt = 0.001f; // don't move on frames where something is amiss with the clock
	}

	last_run_time = current_time;
	// get the 3D ray from the mouse position on the screen
	glm::vec3 ray_origin = window->window_target->camera_position;
	glm::vec3 ray_direction = window->getMouseRay();

	
	glm::vec3 mouse_position = window->window_target->camera_position + window->getMouseRay() * mouse_depth;
	
	glm::mat4 mouse_pose = glm::mat4(1.0f);
	mouse_pose = glm::translate(mouse_pose, mouse_position);
	//cell->setPose(mouse_body, mouse_pose);

	bool clicking = window->mouseDown(1) && !mouse_down_left;
	mouse_down_left = window->mouseDown(1);
	

	updateCamera();
	
	if (clicking) {
		glm::vec3 pos = { min.x + (0.4f + randomFloat() * 0.2f) * (max.x - min.x),12.0f,min.z + 0.5f };
		glm::vec3 vel = { (randomFloat() - 0.5f) * 1.0f,(randomFloat() - 0.5f) * 1.0f,1.0f + randomFloat() * 4.0f };
		glm::mat4 r = glm::rotate(glm::mat4(1.0f), (float)(timeMilliseconds() * 0.002), glm::vec3(0, 1, 0));
		pos = r * glm::vec4(pos, 1);
		vel = r * glm::vec4(vel, 0);
		float rand = randomFloat();
		int type = ball_type;
		if (rand < 0.2f) {
			type = box_type;
		}
		else if (rand < 0.35f) {
			type = rod_type;
		}
		else if (rand < 0.4f) {
			type = bunny_type;
		}
		else if (rand < 0.65) {
			type = jar_type;
		}

		if (cell_id == -1) {
			std::shared_ptr<const NetPhysics::Cell> cell = worlds->observeNearest<NetPhysics::Cell>(WORLD);
			if (cell) {
				cell_id = cell->id;
			}
		}

		if(cell_id != -1){
			int64_t body_id = worlds->create(WORLD, std::make_shared<NetPhysics::RigidBody>(type, pos, vel, glm::vec3(randomFloat() * 2.0f - 1.0f, randomFloat() * 2.0f - 1.0f, randomFloat() * 2.0f - 1.0f)));
			worlds->queue(WORLD, cell_id, &NetPhysics::Cell::addBody, body_id);
		}
	}
	

	// Check if escape pressed to exit
	if (window->getLastKeyPress() == SDLK_ESCAPE) {
		getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
	}
}

// Called when switching out of this state after the last time run is called
void NetPhysicsApp::exit(std::shared_ptr<MachineState> to) {
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
}

void NetPhysicsApp::updateCamera() {
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
	if (zoom < 1.0f) {
		zoom = 1.0f;
	}
	mouse_wheel_y_previous = window->getMouseWheelPosition().y;

	glm::vec3 camera_position = glm::vec3(cosf(camera_theta) * cosf(camera_thi), sinf(camera_thi), sinf(camera_theta) * cosf(camera_thi)) * zoom;
	window->window_target->setCamera(camera_position, look_at, fov, glm::vec3(0, 1, 0));

	glm::vec3 light_position = glm::vec3(cosf(light_theta) * cosf(light_thi), sinf(light_thi), sinf(light_theta) * cosf(light_thi)) * light_zoom;

	scene->moveLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_id, light_position, light_look_at, glm::vec3(0, 1, 0), light_fov, 35);
}

void NetPhysicsApp::createViewTypes(){
	ScenePlugin* scene = getTool<ScenePlugin>();
	glm::vec3 mid = (min + max) * 0.5f;


	float ball_radius = 0.5f;
	float ball_mass = 1.0f;
	std::shared_ptr<Physics::Sphere> ball_shape = std::make_shared<Physics::Sphere>(ball_radius, ball_mass);
	scene->createModelSet(BALL_MODEL, BALL_MODEL, true);
	glm::mat4 transform = glm::scale(glm::mat4(1.0f), glm::vec3(ball_radius, ball_radius, ball_radius));
	ball_type = NetPhysics::RigidBodyView::addType(ball_shape, BALL_MODEL, transform, 0.6f, 0.6f);


	float box_size = 1.0f;
	float box_mass = 2.0f;
	std::shared_ptr<Physics::ConvexPolyhedron> box_shape = std::make_shared<Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeAxisAlignedBox(glm::vec3(box_size, box_size, box_size), box_mass));
	std::shared_ptr<GLTF> box = std::make_shared<GLTF>();
	box->setBoundingBoxModel(glm::vec3(-box_size * 0.5, -box_size * 0.5f, -box_size * 0.5f), glm::vec3(box_size * 0.5f, box_size * 0.5f, box_size * 0.5f), glm::vec4(0.5, 0.5, 1, 1));
	scene->createModelSet("box", box, false, false);
	transform = glm::mat4(1.0f);
	box_type = NetPhysics::RigidBodyView::addType(box_shape, "box", transform, 0.4f, 0.6f);

	float wall_size = 30.0f;
	std::shared_ptr<GLTF> wall = std::make_shared<GLTF>();
	wall->setBoundingBoxModel(glm::vec3(-wall_size * 0.5, -wall_size * 0.5f, -wall_size * 0.5f), glm::vec3(wall_size * 0.5f, wall_size * 0.5f, wall_size * 0.5f), glm::vec4(0.7f, 0.7f, 0.8f, 1));
	scene->createModelSet("wall", wall, false, false);
	std::shared_ptr<Physics::ConvexPolyhedron> wall_shape = std::make_shared<Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeAxisAlignedBox(glm::vec3(wall_size, wall_size, wall_size)));
	wall_type = NetPhysics::RigidBodyView::addType(wall_shape, "wall", transform, 0.6f, 0.6f);

	float rod_mass = 2.0f;
	std::shared_ptr<Physics::ConvexPolyhedron> rod_shape = std::make_shared<Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeCylinder(glm::vec3(0, 0, 1.0f), glm::vec3(0, 0, -1.0f), 0.5f, 16, rod_mass));
	std::shared_ptr<GLTF> model = std::make_shared<GLTF>();
	model->setPolyhedronModel(rod_shape->vertex, rod_shape->face, glm::vec4(0.6f, 0.1f, 0.5f, 0.5f));
	scene->createModelSet("rod", model, false, true);
	rod_type = NetPhysics::RigidBodyView::addType(rod_shape, "rod", transform, 0.4f, 0.2f);


	float jar_mass = 2.0f;
	float jar_scale = 0.6f;
	transform = glm::scale(glm::mat4(1.0f), glm::vec3(jar_scale, jar_scale, jar_scale));
	scene->createModelSet(JAR_MODEL, JAR_MODEL, true);
	std::shared_ptr<GLTF> jar_model = scene->getModelController(JAR_MODEL);
	std::shared_ptr<Physics::ConvexPolyhedron> jar_shape = std::make_shared<Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeApproximateHull(jar_model, jar_mass));
	jar_shape = std::make_shared<Physics::ConvexPolyhedron>(*(jar_shape.get()), transform, jar_mass);
	//std::shared_ptr<GLTF> model2 = std::make_shared<GLTF>();
	//model2->setPolyhedronModel(jar_shape->vertex, jar_shape->face, glm::vec4(0.0f, 0.5f, 0.5f, 0.5f));
	//scene->createModelSet("jar", model2, false, true);
	jar_type = NetPhysics::RigidBodyView::addType(jar_shape, JAR_MODEL, transform, 0.1f, 0.6f);




	float bunny_scale = 1.7f;
	float bunny_mass = 8.0f;
	transform = glm::scale(glm::mat4(1.0f), glm::vec3(bunny_scale, bunny_scale, bunny_scale));
	//transform = glm::rotate(transform, 3.141f,glm::vec3(1,0,0) );
	scene->createModelSet(BUNNY_MODEL, BUNNY_MODEL, true);
	std::shared_ptr<GLTF> bunny_model = scene->getModelController(BUNNY_MODEL);
	std::vector<Physics::ConvexPolyhedron> bunny_parts = Physics::ConvexPolyhedron::makeApproximateSurfaceHulls(bunny_model, bunny_mass, 20, 3);
	scene->createModelSet(BUNNY_VISUAL_MODEL, BUNNY_VISUAL_MODEL, true);
	bunny_type = NetPhysics::RigidBodyView::addType(bunny_parts, BUNNY_VISUAL_MODEL, transform, 0.1f, 0.6f);


	float chain_scale = 0.7f;
	float chain_mass = 0.5f;
	transform = glm::scale(glm::mat4(1.0f), glm::vec3(chain_scale, chain_scale, chain_scale));
	//transform = glm::rotate(transform, 3.141f,glm::vec3(1,0,0) );
	scene->createModelSet(CHAIN_MODEL_CUT, CHAIN_MODEL_CUT, true);
	std::shared_ptr<GLTF> chain_model = scene->getModelController(CHAIN_MODEL_CUT);
	std::vector<Physics::ConvexPolyhedron> chain_parts = Physics::ConvexPolyhedron::makeApproximateSurfaceHulls(chain_model, chain_mass, 20, 3);
	scene->createModelSet(CHAIN_MODEL, CHAIN_MODEL, true);
	chain_type = NetPhysics::RigidBodyView::addType(chain_parts, CHAIN_MODEL, transform, 0.1f, 0.6f);

}


void NetPhysicsApp::host(){
	WorldPlugin* worlds = getTool<WorldPlugin>();

	worlds->createWorld(WORLD, 1E7f, 1E-7f, 1000);
	worlds->setTimeSpeed(WORLD, 1.0f);


	cell_id = worlds->create(WORLD,std::make_shared<NetPhysics::Cell>()) ;
	worlds->queue(WORLD,cell_id,&NetPhysics::Cell::runPhysics) ;


	glm::vec3 mid = (min + max) * 0.5f;
	/*
	glm::vec3 chain_pos = mid;
	float chain_angle = 0;
	float y_step = 0.7f;
	float angle_step = 1.5f;
	int num_links = 15;
	for (int k = 1; k <= num_links; k++) {
		std::shared_ptr<NetPhysics::RigidBody> link = std::make_shared<NetPhysics::RigidBody>(chain_type, chain_pos);
		link->orientation = glm::quat_cast(glm::rotate(glm::mat4(1.0f), chain_angle, glm::vec3(0, 1, 0)));
		if (k == num_links) { // Fix the top link in place
			link->inv_mass = 0;
			link->inv_moment = glm::mat3(0);
		}
		int64_t link_id = worlds->create(WORLD, link);
		worlds->queue(WORLD, cell_id, &NetPhysics::PhysicsCell::addBody, link_id);

		chain_angle += angle_step;
		chain_pos.y += y_step;
		glm::vec3 off((randomFloat() - 0.5f) * 0.3f, (randomFloat() - 0.3f) * 0.1f, (randomFloat() - 0.3f) * 0.1f);
		chain_pos += off;
	}
*/

	int64_t body_id ;
	// Add the container blocks
	float wall_size = 30.0f; // TODO share between type creation
	body_id = worlds->create(WORLD, std::make_shared<NetPhysics::RigidBody>(wall_type, glm::vec3(mid.x, min.y - wall_size * 0.5f, mid.z)));
	worlds->queue(WORLD, cell_id, &NetPhysics::Cell::addBody, body_id);
	
	
	body_id = worlds->create(WORLD, std::make_shared<NetPhysics::RigidBody>(wall_type, glm::vec3(max.x + wall_size * 0.5f, mid.y, mid.z)));
	worlds->queue(WORLD, cell_id, &NetPhysics::Cell::addBody, body_id);
	body_id = worlds->create(WORLD, std::make_shared<NetPhysics::RigidBody>(wall_type, glm::vec3(min.x - wall_size * 0.5f, mid.y, mid.z)));
	worlds->queue(WORLD, cell_id, &NetPhysics::Cell::addBody, body_id);
	body_id = worlds->create(WORLD, std::make_shared<NetPhysics::RigidBody>(wall_type, glm::vec3(mid.x, mid.y, min.z - wall_size * 0.5f)));
	worlds->queue(WORLD, cell_id, &NetPhysics::Cell::addBody, body_id);
	body_id = worlds->create(WORLD, std::make_shared<NetPhysics::RigidBody>(wall_type, glm::vec3(mid.x, mid.y, max.z + wall_size * 0.5f)));
	worlds->queue(WORLD, cell_id, &NetPhysics::Cell::addBody, body_id);
	

	auto box = std::make_shared<NetPhysics::RigidBody>(box_type, glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, 0, 0)) ;
	box->orientation = glm::normalize(glm::quat(0.75,0.5,0.5,0.5)) ;
	body_id = worlds->create(WORLD,box);
	worlds->queue(WORLD, cell_id, &NetPhysics::Cell::addBody, body_id);

	/*
	//Add some random stuff
	for (int k = 0; k < 1; k++) {
		glm::vec3 pos = { min.x + (0.2f + randomFloat() * 0.6f) * (max.x - min.x),min.y + (0.2f + randomFloat() * 0.6f) * (max.y - min.y), min.z + (0.2f + randomFloat() * 0.6f) * (max.z - min.z) };
		glm::vec3 vel = { (randomFloat() - 0.5f) * 1.0f,(randomFloat() - 0.5f) * 1.0f,(randomFloat() - 0.5f) * 1.0f };
		float rand = randomFloat();
		//rand = 1.0f ;
		int type = ball_type;
		if (rand < 0.2f) {
			type = box_type;
		}
		else if (rand < 0.35f) {
			type = rod_type;
		}
		else if (rand < 0.4f) {
			type = bunny_type;
		}
		else if (rand < 0.65) {
			type = jar_type;
		}
		body_id = worlds->create(WORLD, std::make_shared<NetPhysics::RigidBody>(type, pos, glm::vec3(0,0,0), vel));
		worlds->queue(WORLD,cell_id,&NetPhysics::Cell::addBody,body_id) ;
	}
*/
	
	worlds->host(port,version) ;

}
