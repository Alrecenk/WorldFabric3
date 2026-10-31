#include "FirstPersonApp.h"
#include "ParticlePlugin.h"
#include "ScenePlugin.h"
#include "InputPlugin.h"
#include "FlagSet.h"

FirstPersonApp::FirstPersonApp() {}

// Called when switching into this state before the first time run is called
void FirstPersonApp::enter(std::shared_ptr<MachineState> from) {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>();


	NetPhysics2::registerPhysics();
	worlds->registerClass<PlayerCharacter>("FirstPersonPlayer");
	worlds->registerMethod(&PlayerCharacter::update, "PlayerCharacter::update") ;
	worlds->registerMethod(&PlayerCharacter::setPosition, "PlayerCharacter::setPosition");
	worlds->registerMethod(&PlayerCharacter::setControl, "PlayerCharacter::setControl");

	// Set up a light for the scene
	ScenePlugin::LightComponent lc;
	lc.light_color = glm::vec4(0.5, 0.5, 0.5, 1);
	light_id = scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_position, light_look_at, glm::vec3(0, 1, 0), 0.7, 35, 2048, 0, lc);

	// Place the camera
	float fov = 0.7f;
	window->window_target->setCamera({ 0,0,0 }, { 0,0,1}, fov, glm::vec3(0, 1, 0));


	createViewTypes();
	//printf("Attempting to connect to existing simulation...\n");
	if (!SteamworksPlugin::enabled) {
		worlds->connect("127.0.0.1", port, version);
	}

	mouse_action->world = WORLD;
	posed_action_left->world = WORLD;
	posed_action_right->world = WORLD;

	window->setRelativeMouseMode(true) ;




	

	//scene->moveLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_id, light_position, light_look_at, glm::vec3(0, 1, 0), 0.7, 35);


}

//Called every frame while the state is active
void FirstPersonApp::run() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>();
	InputPlugin* input = getTool<InputPlugin>();
	OpenXRPlugin* controls = getTool<OpenXRPlugin>();

	if (!worlds->amHosting() && !worlds->connected()) {
		if (!worlds->connectionPending()) {
			printf("Connection attempt timed out. Hosting...\n");
			host();
		}
		return;
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
	glm::vec3 ray_direction = window->getPixelRay(window->window_target->width*0.5f, window->window_target->height * 0.5f) ;
	bool grabbing = input->getBoolean("grab");
	mouse_action->performAction(ray_origin, ray_direction, grabbing);


	if (OpenXRPlugin::ENABLED) {

		//TODO switch to JSON bindable action instead of direct
		glm::mat4 current_left_hand_pose = controls->getPose("/actions/general/in/left_pose");
		bool grabbing_left = controls->getBoolean("/actions/general/in/press_x");
		posed_action_left->performAction(current_left_hand_pose, grabbing_left);


		glm::mat4 current_right_hand_pose = controls->getPose("/actions/general/in/right_pose");
		bool grabbing_right = controls->getBoolean("/actions/general/in/press_a");
		posed_action_right->performAction(current_right_hand_pose, grabbing_right);

		if (cursor_instance_left == -1) {
			scene->createModelSet(CURSOR_MODEL, CURSOR_MODEL, false);

			cursor_instance_left = scene->createInstance(CURSOR_MODEL, current_left_hand_pose * cursor_adjust);
			cursor_instance_right = scene->createInstance(CURSOR_MODEL, current_right_hand_pose * cursor_adjust);
		}

		cursor_adjust = glm::mat4(1.0f);
		cursor_adjust = glm::scale(cursor_adjust, glm::vec3(0.01f, 0.01f, 0.01f));
		cursor_adjust = glm::rotate(cursor_adjust, cursor_angle_1, glm::vec3(0.00f, 1.0f, 0.0f));
		cursor_adjust = glm::rotate(cursor_adjust, cursor_angle_2, glm::vec3(1.00f, 0.0f, 0.0f));

		scene->setPose(cursor_instance_left, current_left_hand_pose * cursor_adjust);
		scene->setPose(cursor_instance_right, current_right_hand_pose * cursor_adjust);


	}


	updateCamera();

	if (input->getPressed("create")) {
		glm::vec3 pos = { min.x + (0.4f + randomFloat() * 0.2f) * (max.x - min.x),2.0f,min.z + 0.1f };
		glm::vec3 vel = { (randomFloat() - 0.5f) * 0.1f,(randomFloat() - 0.5f) * 0.1f,1.0f + randomFloat() * 0.5f };
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

		if (root_id == -1) {
			std::shared_ptr<const NetPhysics2::Root> root = worlds->observeNearest<NetPhysics2::Root>(WORLD);
			if (root) {
				root_id = root->id;
			}
		}

		if (root_id != -1) {
			int64_t body_id = worlds->create(WORLD, std::make_shared<NetPhysics2::RigidBody>(root_id, type, pos, glm::quat(1, 0, 0, 0), vel, glm::vec3(randomFloat() * 2.0f - 1.0f, randomFloat() * 2.0f - 1.0f, randomFloat() * 2.0f - 1.0f)));
			worlds->queue(WORLD, body_id, &NetPhysics2::RigidBody::runPhysics);
		}
	}
	space_down = window->keyDown(SDLK_SPACE);


	// Check if escape pressed to exit
	if (input->getPressed("exit")) {
		getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
	}

	glm::vec3 new_control;
	glm::vec3 look = glm::vec3(cosf(camera_theta) * cosf(camera_thi), sinf(camera_thi), sinf(camera_theta) * cosf(camera_thi));
	glm::vec3 right = glm::cross(look, glm::vec3(0,1,0) );

	if (input->getBoolean(input->getActionID("forward"))) {
		new_control += look ;
	}

	if (input->getBoolean(input->getActionID("backward"))) {
		new_control -= look ;
	}

	if (input->getBoolean(input->getActionID("left"))) {
		new_control -= right;
	}

	if (input->getBoolean(input->getActionID("right"))) {
		new_control += right;
	}

	float s= glm::length(new_control) ;
	if(s > 0){
		new_control *= PlayerCharacter::acceleration /s ;
	}
	
	if(new_control != last_control || millisBetween(last_input_time, now()) > PlayerCharacter::time_out_time * 500){
		if(my_player_id == -1 && new_control != last_control){
			
			my_player_id = worlds->create(WORLD, std::make_shared<PlayerCharacter>(root_id, glm::vec3(0,0,0))) ;
			worlds->queue(WORLD, my_player_id, &PlayerCharacter::update) ;
		}else if(my_player_id != -1 ) {
			worlds->queue(WORLD, my_player_id,&PlayerCharacter::setControl,new_control) ;
		}
	}

}

// Called when switching out of this state after the last time run is called
void FirstPersonApp::exit(std::shared_ptr<MachineState> to) {
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
}

void FirstPersonApp::updateCamera() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>();
	
	glm::vec2 mouse_delta = window->getMouseMovement();
	window->consumeMouseMovement();
	
	camera_theta += camera_x_speed * mouse_delta.x;
	camera_thi += camera_y_speed * mouse_delta.y;
	camera_thi = fmax(fmin(camera_thi, 3.1415f * 0.5f), -3.1415f*0.5f);
	
	std::shared_ptr<const PlayerCharacter> player = worlds->observe<PlayerCharacter>(WORLD,my_player_id) ;
		
	glm::vec3 camera_position = player ? player->position : glm::vec3(0, 0, 0);
	glm::vec3 look_at = camera_position + glm::vec3(cosf(camera_theta) * cosf(camera_thi), sinf(camera_thi), sinf(camera_theta) * cosf(camera_thi));
	
	window->window_target->setCamera(camera_position, look_at, fov, glm::vec3(0, 1, 0));

	updateDebugPanel();

}

void FirstPersonApp::createViewTypes() {
	ScenePlugin* scene = getTool<ScenePlugin>();
	glm::vec3 mid = (min + max) * 0.5f;


	float ball_radius = 0.1f;
	float ball_mass = 1.0f;
	std::shared_ptr<Physics::Sphere> ball_shape = std::make_shared<Physics::Sphere>(ball_radius, ball_mass);
	scene->createModelSet(BALL_MODEL, BALL_MODEL, true);
	glm::mat4 transform = glm::scale(glm::mat4(1.0f), glm::vec3(ball_radius, ball_radius, ball_radius));
	ball_type = NetPhysics2::RigidBody::addType(ball_shape, BALL_MODEL, transform, 0.6f, 0.6f);


	float box_size = 0.15f;
	float box_mass = 2.0f;
	std::shared_ptr<Physics::ConvexPolyhedron> box_shape = std::make_shared<Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeAxisAlignedBox(glm::vec3(box_size, box_size, box_size), box_mass));
	std::shared_ptr<GLTF> box = std::make_shared<GLTF>();
	box->setBoundingBoxModel(glm::vec3(-box_size * 0.5, -box_size * 0.5f, -box_size * 0.5f), glm::vec3(box_size * 0.5f, box_size * 0.5f, box_size * 0.5f), glm::vec4(0.5, 0.5, 1, 1));
	scene->createModelSet("box", box, false, false);
	transform = glm::mat4(1.0f);
	box_type = NetPhysics2::RigidBody::addType(box_shape, "box", transform, 0.4f, 0.6f);

	std::shared_ptr<GLTF> wall = std::make_shared<GLTF>();
	wall->setBoundingBoxModel(glm::vec3(-wall_size * 0.5, -wall_size * 0.5f, -wall_size * 0.5f), glm::vec3(wall_size * 0.5f, wall_size * 0.5f, wall_size * 0.5f), glm::vec4(0.7f, 0.7f, 0.8f, 1));
	scene->createModelSet("wall", wall, false, false);
	std::shared_ptr<Physics::ConvexPolyhedron> wall_shape = std::make_shared<Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeAxisAlignedBox(glm::vec3(wall_size, wall_size, wall_size)));
	wall_type = NetPhysics2::RigidBody::addType(wall_shape, "wall", transform, 0.6f, 0.6f);

	float rod_mass = 2.0f;
	float rod_scale = 0.12f;
	transform = glm::mat4(1.0f);
	std::shared_ptr<Physics::ConvexPolyhedron> rod_shape = std::make_shared<Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeCylinder(glm::vec3(0, 0, rod_scale), glm::vec3(0, 0, -rod_scale), rod_scale * 0.5f, 16, rod_mass));
	std::shared_ptr<GLTF> model = std::make_shared<GLTF>();
	model->setPolyhedronModel(rod_shape->vertex, rod_shape->face, glm::vec4(0.6f, 0.1f, 0.5f, 0.5f));
	scene->createModelSet("rod", model, false, true);
	rod_type = NetPhysics2::RigidBody::addType(rod_shape, "rod", transform, 0.4f, 0.2f);


	float jar_mass = 2.0f;
	float jar_scale = 0.1f;
	transform = glm::scale(glm::mat4(1.0f), glm::vec3(jar_scale, jar_scale, jar_scale));
	scene->createModelSet(JAR_MODEL, JAR_MODEL, true);
	std::shared_ptr<GLTF> jar_model = scene->getModelController(JAR_MODEL);
	std::shared_ptr<Physics::ConvexPolyhedron> jar_shape = std::make_shared<Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeApproximateHull(jar_model, jar_mass));
	jar_shape = std::make_shared<Physics::ConvexPolyhedron>(*(jar_shape.get()), transform, jar_mass);
	//std::shared_ptr<GLTF> model2 = std::make_shared<GLTF>();
	//model2->setPolyhedronModel(jar_shape->vertex, jar_shape->face, glm::vec4(0.0f, 0.5f, 0.5f, 0.5f));
	//scene->createModelSet("jar", model2, false, true);
	jar_type = NetPhysics2::RigidBody::addType(jar_shape, JAR_MODEL, transform, 0.1f, 0.6f);

	float bunny_scale = 0.25f;
	float bunny_mass = 8.0f;
	transform = glm::scale(glm::mat4(1.0f), glm::vec3(bunny_scale, bunny_scale, bunny_scale));
	//transform = glm::rotate(transform, 3.141f,glm::vec3(1,0,0) );
	scene->createModelSet(BUNNY_MODEL, BUNNY_MODEL, true);
	std::shared_ptr<GLTF> bunny_model = scene->getModelController(BUNNY_MODEL);
	std::vector<Physics::ConvexPolyhedron> bunny_parts = Physics::ConvexPolyhedron::makeApproximateSurfaceHulls(bunny_model, bunny_mass, 20, 3);
	scene->createModelSet(BUNNY_VISUAL_MODEL, BUNNY_VISUAL_MODEL, true);
	bunny_type = NetPhysics2::RigidBody::addType(bunny_parts, BUNNY_VISUAL_MODEL, transform, 0.1f, 0.6f);


	float ruin_scale = 0.9f;
	transform = glm::scale(glm::mat4(1.0f), glm::vec3(ruin_scale, ruin_scale, ruin_scale));
	//transform = glm::rotate(transform, 3.141f,glm::vec3(1,0,0) );
	scene->createModelSet(RUIN_MODEL, RUIN_MODEL, false);
	std::shared_ptr<GLTF> ruin_model = scene->getModelController(RUIN_MODEL);
	std::vector<Physics::ConvexPolyhedron> ruin_parts = Physics::ConvexPolyhedron::collectConvexPiecesByBSP(ruin_model);
	scene->createModelSet(RUIN_VISUAL_MODEL, RUIN_VISUAL_MODEL, false);
	ruin_type = NetPhysics2::RigidBody::addType(ruin_parts, RUIN_VISUAL_MODEL, transform, 0.1f, 0.6f);



	float player_radius = 0.2f ;
	float player_height = 1.5;
	transform = glm::mat4(1.0f) ;
	std::shared_ptr<Physics::ConvexPolyhedron> player_shape = std::make_shared<Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeCylinder(
		glm::vec3(0, -player_height *0.75f, 0), 
		glm::vec3(0, player_height*0.25f, 0),
		player_radius,
		8));
	std::shared_ptr<GLTF> player_model = std::make_shared<GLTF>();
	player_model->setPolyhedronModel(player_shape->vertex, player_shape->face, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
	scene->createModelSet("player", player_model, false, false);
	player_type = NetPhysics2::RigidBody::addType(player_shape, "player", transform, 0.4f, 0.2f);
}


void FirstPersonApp::host() {
	WorldPlugin* worlds = getTool<WorldPlugin>();
	worlds->clearWorlds();
	worlds->createWorld(WORLD, 1E4f, 1E-4f, 10);
	worlds->setTimeSpeed(WORLD, 1.0f);


	root_id = worlds->create(WORLD, std::make_shared<NetPhysics2::Root>(cell_size));


	glm::vec3 mid = (min + max) * 0.5f;
	
	int64_t body_id;
	// Add the container blocks
	/*
	body_id = worlds->create(WORLD, std::make_shared<NetPhysics2::RigidBody>(root_id, wall_type, glm::vec3(mid.x, min.y - wall_size * 0.5f, mid.z)));
	worlds->queue(WORLD, body_id, &NetPhysics2::RigidBody::runPhysics);


	body_id = worlds->create(WORLD, std::make_shared<NetPhysics2::RigidBody>(root_id, wall_type, glm::vec3(max.x + wall_size * 0.5f, mid.y, mid.z)));
	worlds->queue(WORLD, body_id, &NetPhysics2::RigidBody::runPhysics);
	body_id = worlds->create(WORLD, std::make_shared<NetPhysics2::RigidBody>(root_id, wall_type, glm::vec3(min.x - wall_size * 0.5f, mid.y, mid.z)));
	worlds->queue(WORLD, body_id, &NetPhysics2::RigidBody::runPhysics);
	body_id = worlds->create(WORLD, std::make_shared<NetPhysics2::RigidBody>(root_id, wall_type, glm::vec3(mid.x, mid.y, min.z - wall_size * 0.5f)));
	worlds->queue(WORLD, body_id, &NetPhysics2::RigidBody::runPhysics);
	body_id = worlds->create(WORLD, std::make_shared<NetPhysics2::RigidBody>(root_id, wall_type, glm::vec3(mid.x, mid.y, max.z + wall_size * 0.5f)));
	worlds->queue(WORLD, body_id, &NetPhysics2::RigidBody::runPhysics);
	*/

	body_id = worlds->create(WORLD, std::make_shared<NetPhysics2::RigidBody>(root_id, ruin_type, glm::vec3(mid.x, min.y - wall_size * 0.5f, mid.z)));
	worlds->queue(WORLD, body_id, &NetPhysics2::RigidBody::runPhysics);
	

	//Add some random stuff
	int amount = 10;
	for (int k = 0; k < amount; k++) {
		glm::vec3 pos = { min.x + (0.2f + randomFloat() * 0.6f) * (max.x - min.x),min.y + (0.6f + randomFloat() * 0.4f) * (max.y - min.y), min.z + (0.2f + randomFloat() * 0.6f) * (max.z - min.z) };
		glm::vec3 vel = { (randomFloat() - 0.5f) * 1.0f,(randomFloat() - 0.5f) * 1.0f,(randomFloat() - 0.5f) * 1.0f };
		float rand = k / (float)amount;
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
		body_id = worlds->create(WORLD, std::make_shared<NetPhysics2::RigidBody>(root_id, type, pos, glm::quat(1, 0, 0, 0), vel));
		worlds->queue(WORLD, body_id, &NetPhysics2::RigidBody::runPhysics);
	}


	if (SteamworksPlugin::enabled) {
		startSteamHosting();
	}
	else {
		worlds->host(port, version);
	}

}


void FirstPersonApp::startSteamHosting() {
	SteamworksPlugin* steam = getTool<SteamworksPlugin>();
	WorldPlugin* world = getTool<WorldPlugin>();
	SteamworksPlugin::SteamServerInfo info;

	steam->setSteamEventReceiver(this);

	info.name = "PhysicsTest";
	info.map = "pit";
	info.max_players = 64;
	info.game_mode = "sdfg";
	info.product_name = "sdfgs";
	info.product_description = "ghhh";
	info.game_directory = "fghjkhgkj";
	info.version = "1";
	std::shared_ptr<Socket> steam_socket = steam->hostPrivateLobby(info);

	world->host(steam_socket, info.version);
}

void FirstPersonApp::updateDebugPanel() {
	PanelPlugin* panels = getTool<PanelPlugin>();
	VulkanPlugin* window = getTool<VulkanPlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>();

	int panel_width = 740;
	int panel_height = 130;

	if (!debug_panel_enabled) {
		if (debug_panel != -1) {
			debug_label.reset();
			panels->deletePanel(debug_panel);
			debug_panel = -1;
		}
		return;
	}

	if (debug_panel == -1) {
		debug_panel = panels->createPanel(panel_width, panel_height, { 0.0,0.0,0.0,0.0 });
		debug_label = std::shared_ptr<PanelPlugin::Label>(new PanelPlugin::Label(debug_panel, "Debug Info", panel_width * 0.5f, 10, true, "arial"));
		debug_label->setColors(glm::vec4(0), glm::vec4(0), glm::vec4(0, 0, 0, 1), glm::vec4(0));

	}
	else {
		total_dt += worlds->getTimeStep();
		debug_frames++;
		if (total_dt > 1.0) {
			debug_fps = (int)(debug_frames / total_dt);
			debug_frames = 0;
			total_dt = 0;

			std::string d = concat("fps:", debug_fps);
			if (worlds->getPing() != 0) {
				d += concat(" ping:", (int)(1000 * worlds->getPing()));
			}
			//printf("%s\n", d.c_str()) ;
			debug_label->setText(d);
		}
	}

	float screen_x = window->window_target->width / 10.0f;
	float screen_y = screen_x * panel_height / panel_width;

	float debug_depth = 1.0f;
	float size = 30.0f;

	glm::vec3 Z = window->getPixelRay(screen_x, screen_y) * debug_depth;
	glm::vec3 X = window->getPixelRay(screen_x + size * (debug_label->image_width + 200) / debug_label->image_height, screen_y) * debug_depth - Z;
	glm::vec3 Y = window->getPixelRay(screen_x, screen_y - size) * debug_depth - Z;
	glm::vec3 origin = window->window_target->camera_position + Z - X * 0.5f - Y * 0.5f;
	PanelPlugin::DefaultInstance inst = panels->getPanelInstance<PanelPlugin::DefaultInstance>(debug_panel);
	inst.pose = panels->getPose(origin, X, Y);


	panels->setPanelInstance(debug_panel, inst);
	//printf("o:%f,%f,%f  x:%f,%f,%f  Y:%f,%f,%f \n", origin.x, origin.y, origin.z, X.x,X.y,X.z,Y.x,Y.y,Y.z) ;



}

void FirstPersonApp::onSteamGameExternalJoin(std::shared_ptr<SteamworksPlugin::SteamSocket> socket, const SteamworksPlugin::SteamServerInfo& server_info) {
	WorldPlugin* world = getTool<WorldPlugin>();
	SteamworksPlugin* steam = getTool<SteamworksPlugin>();
	world->disconnect();
	world->connect(socket, "1");

}

PlayerCharacter::PlayerCharacter(int64_t root, const glm::vec3& pos){
	root_id = root ;
	position = pos ;
}

void PlayerCharacter::update() {
	double frame_length = 1.0 / NetPhysics2::Root::ticks_per_second;
	int frame = (int)(time * NetPhysics2::Root::ticks_per_second);
	double target_time = (frame + interframe_time) * frame_length;

	if(body_id == -1){
		body_id = create(std::make_shared<NetPhysics2::RigidBody>(root_id, FirstPersonApp::player_type, position), time);
		queue(body_id,time+0.001, &NetPhysics2::RigidBody::runPhysics);
		last_control_time = time;
	}else{
		
		velocity+=control*frame_length;
		float initial_speed = glm::length(velocity) ;
		float speed = fmin(initial_speed-drag*frame_length,max_speed) ;
		if(speed <= 0){
			velocity = glm::vec3(0,0,0) ;
		}else{
			velocity *= speed/initial_speed ;
		}
		position += velocity*frame_length ;
		
		//TODO check collision

		queue(body_id,target_time,&NetPhysics2::RigidBody::setState,position,velocity, glm::quat(1,0,0,0),glm::vec3(0,0,0)) ;

		if(time - last_control_time > time_out_time){
			queue(body_id,time, &NetPhysics2::RigidBody::destroy) ;
			destroyed= true ;
		}
		
	}
	if(!destroyed){
		queue(id,target_time + frame_length, &PlayerCharacter::update) ;
	}
}

void PlayerCharacter::setControl(const glm::vec3& new_control) {
	control = new_control;
	last_control_time = time;
}

void PlayerCharacter::setPosition(const glm::vec3& new_pos) {
	position = new_pos ;
	velocity = glm::vec3(0,0,0) ;
	last_control_time = time;
}