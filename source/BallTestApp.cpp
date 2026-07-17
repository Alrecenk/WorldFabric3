#include "BallTestApp.h"
#include "ParticlePlugin.h"
#include "VulkanDemoApp.h" // TODO don't rely on demo app code
#include "FlagSet.h"

//Loads models from the hard drive on construction
BallTestApp::BallTestApp() {

	registry->registerClass<Ball>("Ball");
	Ball::SET_POSITION= registry->registerMethod(&Ball::setPosition, "setPosition");
	Ball::UPDATE = registry->registerMethod(&Ball::update,"update");
	Ball::SET_VELOCITY = registry->registerMethod(&Ball::setVelocity,"setVelocity");
	Ball::APPLY_IMPULSE= registry->registerMethod(&Ball::applyImpulse, "applyImpulse");
	registry->registerClass<Cell>("Cell");
	Cell::ADD_BALL = registry->registerMethod(&Cell::addBall, "addBall");
	Cell::REMOVE_BALL = registry->registerMethod(&Cell::removeBall, "removeBall");
	registry->registerClass<Grid>("Grid");
	Grid::CREATE_GRID = registry->registerMethod(&Grid::createGrid, "createGrid");

	auto grid = std::make_shared<Grid>(glm::vec3(min_floor.x,-1, min_floor.y), glm::vec3(max_floor.x, 0, max_floor.y),grid_size,grid_size);
	grid_id = host_timeline.create(host_vantage, 0, grid, 0.1f);
	host_timeline.queue(host_vantage, 0, grid_id, 0.6, &Grid::createGrid);
	for (int k = 0; k < num_balls; k++) {
		createBall(120.0/host_timeline.max_info_speed);
	/*
		glm::vec3 p = glm::vec3(min_floor.x + ball_radius + randomFloat() * (max_floor.x - min_floor.x - ball_radius * 2), -ball_radius,
			min_floor.y + ball_radius + randomFloat() * (max_floor.y - min_floor.y - ball_radius * 2));
		glm::vec3 v = glm::vec3(randomFloat() - 0.5f, 0, randomFloat() - 0.5f);
		v *= ball_speed / glm::length(v);
		//host_timeline.queue(host_vantage, 0, grid_id, 1.0f+k* Timeline::min_event_duration, &Grid::createBall, p, v, ball_radius);
		
		auto ball = std::make_shared<Ball>(p, v, ball_radius, glm::vec3(randomFloat()*0.8f, randomFloat() * 0.8f, randomFloat() * 0.8f), grid_id);
		int64_t ball_id = host_timeline.create(host_vantage, 0, ball, 0.5f);
		host_timeline.queue(host_vantage, 0, ball_id, (0.8f + randomFloat() * 0.1f) * host_timeline.history_kept, &Ball::update);
	*/
	}


	auto host_player = std::make_shared<Ball>(glm::vec3(4, -ball_radius, 4), last_host_valocity, player_radius, glm::vec3(0.2f, 0.2f, 0.8f), grid_id);
	host_player->player = true;
	host_player_id = host_timeline.create(host_vantage, 0, host_player, 1.0f);
	host_timeline.queue(host_vantage, 0, host_player_id, (0.8f + randomFloat() * 0.1f) * 3.0, &Ball::update);


	auto client_player = std::make_shared<Ball>(glm::vec3(-4, -ball_radius, -4), last_client_valocity, player_radius, glm::vec3(0.8f, 0.2f, 0.2f), grid_id);
	client_player->player = true;
	client_player_id = host_timeline.create(host_vantage, 0, client_player, 1.0f);
	host_timeline.queue(host_vantage, 0, client_player_id, (0.8f + randomFloat() * 0.1f)* 3.0, &Ball::update);


	host_timeline.run(host_vantage, 6.0);
	host_time = 6.0;


}


void BallTestApp::createBall(double time){
	glm::vec3 p = glm::vec3(min_floor.x + ball_radius + randomFloat() * (max_floor.x - min_floor.x - ball_radius * 2), -ball_radius,
		min_floor.y + ball_radius + randomFloat() * (max_floor.y - min_floor.y - ball_radius * 2));
	glm::vec3 v = glm::vec3(randomFloat() - 0.5f, 0, randomFloat() - 0.5f);
	v *= ball_speed / glm::length(v);
	auto ball = std::make_shared<Ball>(p, v, ball_radius, glm::vec3(randomFloat() * 0.8f, randomFloat() * 0.8f, randomFloat() * 0.8f), grid_id);
	int64_t ball_id = host_timeline.create(host_vantage, time, ball, time+0.1 + randomFloat()*0.05f);
	printf("Creating ball with id: %lld\n", ball_id) ;
	
	host_timeline.queue(host_vantage, 0, ball_id, time + (randomFloat()+1.5)  * 0.1f, &Ball::update);
	

}


// Called when switching into this sate before the first time run is claled
void BallTestApp::enter(std::shared_ptr<MachineState> from) {
	last_run_time = now();
}


// Called when switching outof this state after the last time run is called
void BallTestApp::exit(std::shared_ptr<MachineState> to) {
	VulkanPlugin* renderer = getTool<VulkanPlugin>();
	AudioPlugin* audio = getTool<AudioPlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	for (auto& [t_id, p_id] : host_ball_particle) {
		particles->destroyParticle(p_id);
	}

	for (auto& [t_id, p_id] : client_ball_particle) {
		particles->destroyParticle(p_id);
	}

}

// Called when switching into this sate before the first time run is claled
void BallTestApp::run() {

	VulkanPlugin* renderer = getTool<VulkanPlugin>();
	AudioPlugin* audio = getTool<AudioPlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();



	//window->setView(view_position, view_Z);
	float fov = 0.5f;
	glm::mat4 look_at = glm::lookAt(view_position, view_position + view_Z, glm::vec3(0, 1, 0));
	glm::mat4 projection = glm::perspective(fov, renderer->window_target->width / (float)renderer->window_target->height, 0.1f, 1000.0f);
	//glm::mat4 rotate = glm::rotate(glm::mat4(1.0f), ts, glm::vec3(0, 1, 0));
	renderer->window_target->camera_matrix = projection * look_at; // * rotate;

	if (renderer->getLastKeyPress() == SDLK_ESCAPE) {
		getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
	}

	glm::vec3 new_client_velocity = glm::vec3(0, 0, 0);
	if (renderer->keyDown(SDLK_UP)) {
		new_client_velocity.z += 1.0f;
	}
	if (renderer->keyDown(SDLK_DOWN)) {
		new_client_velocity.z -= 1.0f;
	}
	if (renderer->keyDown(SDLK_LEFT)) {
		new_client_velocity.x += 1.0f;
	}
	if (renderer->keyDown(SDLK_RIGHT)) {
		new_client_velocity.x -= 1.0f;
	}
	float l = glm::length(new_client_velocity);
	if (l > 0.01f) {
		new_client_velocity *= player_speed / l;
	}
	if (new_client_velocity != last_client_valocity) {
		client_timeline.queue(client_vantage, client_time, client_player_id, client_time + Ball::tick_interval, &Ball::setVelocity, new_client_velocity);
		last_client_valocity = new_client_velocity;
		//printf("changing host velocity: %f,%f,%f\n", new_host_velocity.x, new_host_velocity.y, new_host_velocity.z);
	}


	glm::vec3 new_host_velocity = glm::vec3(0, 0, 0);
	if (renderer->keyDown(SDLK_w)) {
		new_host_velocity.z += 1.0f;
	}
	if (renderer->keyDown(SDLK_s)) {
		new_host_velocity.z -= 1.0f;
	}
	if (renderer->keyDown(SDLK_a)) {
		new_host_velocity.x += 1.0f;
	}
	if (renderer->keyDown(SDLK_d)) {
		new_host_velocity.x -= 1.0f;
	}
	l = glm::length(new_host_velocity);
	if (l > 0.01f) {
		new_host_velocity *= player_speed / l;
	}
	if (new_host_velocity != last_host_valocity) {
		//printf("queuing move event!\n");
		host_timeline.queue(host_vantage, host_time, host_player_id, host_time + Ball::tick_interval, &Ball::setVelocity, new_host_velocity);
		last_host_valocity = new_host_velocity;
		//printf("changing host velocity: %f,%f,%f\n", new_host_velocity.x, new_host_velocity.y, new_host_velocity.z);
	}
		

	if (renderer->keyDown(SDLK_SPACE)) {
		if (!space_held) { // onoly act on initial press not every frame wile held
			//client_timeline = Timeline(registry, max_info_speed, min_event_duration, history_kept); // make a new timeline so we can retry the initial sync looking for errors  without rebooting the app
			if (sync_state != 0) {
				//printf("disconnecting!\n");
				sync_state = 0;
			}else{
				/*
				double sync_time = host_time - sync_age;
				auto descriptor2 = client_timeline.getDescriptor(sync_time, true);
				auto update = host_timeline.getUpdateFor(descriptor2,sync_depth, true);
				client_timeline.applyUpdate(update);
				
				*/
				double sync_time = host_time - sync_age;
				printf("Sync time: %lf Copy time: %lf\n", sync_time, sync_time - sync_depth) ;
				auto update = host_timeline.copy(sync_time-sync_depth) ;
				client_timeline = Timeline(registry, update);
				
			

				client_time = host_time;
				client_vantage = host_vantage ;
				client_timeline.run(client_vantage, client_time);
				host_timeline.run(host_vantage, host_time);
				sync_state = 1;
				last_sync_time = now();
				
				checkBaseObjects(sync_time);
				
				/*
				for(int k=0;k<100;k++){
					updateParticles(host_timeline, host_vantage, host_time, host_transform_group, host_ball_particle, particles);
					updateParticles(client_timeline, client_vantage, client_time, client_transform_group, client_ball_particle, particles);
					client_time += 0.1 ;
					host_time += 0.1 ;

					//printf("Client run: %lf\n", client_time) ;
					client_timeline.run(client_vantage, client_time);
					//printf("Host run: %lf\n", host_time);
					host_timeline.run(host_vantage, host_time);
					sync_time = host_time - sync_age;
					//checkBaseEvents(sync_time);
					checkBaseObjects(sync_time);
				}
				*/
			}
		}
		space_held = true;
	}
	else {
		space_held = false;
	}


	if (renderer->keyDown(SDLK_RETURN)){
		if(!enter_held){
			createBall(host_time);
		}
		enter_held = true;
	}else{
		enter_held = false;
	}

	
	if (host_floor_instance == -1) {
		
		host_transform = glm::mat4(1.0f);

		host_transform = glm::translate(host_transform, glm::vec3(0.6, 1, 3));
		host_transform = glm::scale(host_transform, glm::vec3(0.1f, 0.1f, 0.1f));
		host_transform = glm::rotate(host_transform, 3.14159f * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));

		host_floor_instance = createBlock(renderer, host_transform_group, glm::vec3(min_floor.x,0, min_floor.y), glm::vec3(max_floor.x, 1.0f, max_floor.y));
		wall_instances.push_back(createBlock(renderer, host_transform_group, glm::vec3(min_floor.x-1.0f,  -1.0f, min_floor.y ), glm::vec3(min_floor.x, 0.0f,max_floor.y)));
		wall_instances.push_back(createBlock(renderer, host_transform_group, glm::vec3(max_floor.x, -1.0f, min_floor.y), glm::vec3(max_floor.x+1.0f, 0.0f, max_floor.y)));
		wall_instances.push_back(createBlock(renderer, host_transform_group, glm::vec3(min_floor.x, -1.0f, min_floor.y-1.0f), glm::vec3(max_floor.x, 0.0f, min_floor.y)));
		wall_instances.push_back(createBlock(renderer, host_transform_group, glm::vec3(min_floor.x, -1.0f, max_floor.y), glm::vec3(max_floor.x, 0.0f, max_floor.y + 1.0f)));
		
		
		
		//scene->setGroupTransform(host_transform_group, host_transform);
		particles->setGroupTransform(host_transform_group, host_transform);



		client_transform = glm::mat4(1.0f);

		client_transform = glm::translate(client_transform, glm::vec3(-0.6, 1, 3));
		client_transform = glm::scale(client_transform, glm::vec3(0.1f, 0.1f, 0.1f));
		client_transform = glm::rotate(client_transform, 3.14159f * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
		
		client_floor_instance = createBlock(renderer, client_transform_group, glm::vec3(min_floor.x, 0, min_floor.y), glm::vec3(max_floor.x, 1.0f, max_floor.y));
		wall_instances.push_back(createBlock(renderer, client_transform_group, glm::vec3(min_floor.x - 1.0f, -1.0f, min_floor.y), glm::vec3(min_floor.x, 0.0f, max_floor.y)));
		wall_instances.push_back(createBlock(renderer, client_transform_group, glm::vec3(max_floor.x, -1.0f, min_floor.y), glm::vec3(max_floor.x + 1.0f, 0.0f, max_floor.y)));
		wall_instances.push_back(createBlock(renderer, client_transform_group, glm::vec3(min_floor.x, -1.0f, min_floor.y - 1.0f), glm::vec3(max_floor.x, 0.0f, min_floor.y)));
		wall_instances.push_back(createBlock(renderer, client_transform_group, glm::vec3(min_floor.x, -1.0f, max_floor.y), glm::vec3(max_floor.x, 0.0f, max_floor.y + 1.0f)));
		

		
		//scene->setGroupTransform(client_transform_group, client_transform);
		particles->setGroupTransform(client_transform_group, client_transform);

	}

	
	auto current_time = now();
	//float dt = microsBetween(last_run_time, current_time) / 1000000.0f;
	float dt = 0.007f;
	last_run_time = current_time;
	host_time += dt;
	//printf("running to %f\n", host_time);
	host_timeline.run(host_vantage, host_time);


	client_time = host_time;
	//printf("running to %f\n", host_time);
	client_timeline.run(client_vantage, client_time);

	updateParticles(host_timeline, host_vantage, host_time, host_transform_group, host_ball_particle, particles);
	updateParticles(client_timeline, client_vantage, client_time, client_transform_group, client_ball_particle, particles);
	


	if (microsBetween(last_sync_time, current_time) > ping * 500) {// round trip milisecond ot micros
		double hash_time = (int)((host_time)*4 - 1)/4.0 ;
		if (sync_state == 1) {
			if(client_update.hash_time > 0){
				host_timeline.applyUpdate(client_update);
			}
			host_timeline.run(host_vantage, host_time);
			host_update = host_timeline.getPendingUpdate() ;
			
			sync_state = 2;


		}
		else if (sync_state == 2) {
			if (host_update.hash_time > 0) {
				client_timeline.applyUpdate(host_update);
			}
			client_timeline.run(client_vantage, client_time);
			client_update = client_timeline.getPendingUpdate();
			sync_state = 1;
		}

		last_sync_time = current_time;
	}
}

void BallTestApp::updateParticles(Timeline& timeline, glm::vec3& vantage, double time,  int transform_group, std::unordered_map<int64_t, int>& ball_particle, ParticlePlugin* particles) {
	std::vector<std::shared_ptr<const Timeline::WorldObject>> observed = timeline.observe(vantage, time);

	for (auto& tobj : observed) {
		std::shared_ptr<const Ball> ball = dynamic_pointer_cast<const Ball>(tobj);
		if (ball != nullptr) { // if the observed object is a ball
			//printf("ball at (%f,%f%f) radius %f\n", ball->position.x, ball->position.y, ball->position.z, ball->radius);
			auto inst_it = ball_particle.find(ball->id);
			if (inst_it == ball_particle.end()) { // no particle
				ball_particle[ball->id] = particles->createParticle(transform_group);
				particles->setColor(ball_particle[ball->id], glm::vec4(ball->color.r, ball->color.g, ball->color.b, 1.0f));
				//printf("made a particle %d\n", ball_particle[ball->id]);
			}

			//float timeat = host_time - glm::length(ball->position - host_vantage) / Timeline::max_info_speed;

			//glm::vec3 predicted = ball->position + ball->velocity * (timeat - ball->time);

			glm::vec3 predicted = ball->position;

			glm::mat4 ball_transform = glm::mat4(1.0f);

			ball_transform = glm::translate(ball_transform, predicted);
			ball_transform = glm::scale(ball_transform, glm::vec3(ball->radius, ball->radius, ball->radius));

			particles->setPose(ball_particle[ball->id], ball_transform);


			if (transform_group == host_transform_group && ball->id == host_player_id) {
				host_vantage = ball->position;
			}
			if (transform_group == client_transform_group && ball->id == client_player_id) {
				client_vantage = ball->position;
			}
		}

	}
}


int BallTestApp::createBlock(VulkanPlugin* renderer, int transform_group, glm::vec3 min, glm::vec3 max){
	
	if(meshes.size() == 0){


		// Load the shader for the mesh pipeline
		Variant vertex_shader_file_data = Variant::loadFileBytes("./shader/colored_triangle_mesh.vert.spv");
		VkShaderModule triangleVertexShader = renderer->loadShader(vertex_shader_file_data.getByteArray(), vertex_shader_file_data.getArrayLength());
		Variant frag_shader_file_data = Variant::loadFileBytes("./shader/colored_triangle.frag.spv");
		VkShaderModule triangleFragShader = renderer->loadShader(frag_shader_file_data.getByteArray(), frag_shader_file_data.getArrayLength());
		int num_textures = 1;
		mesh_program = std::shared_ptr<TriangleShaderProgram>( new TriangleShaderProgram(
			renderer->device,
			triangleVertexShader,
			triangleFragShader,
			sizeof(MeshPushConstants),
			num_textures,
			VK_CULL_MODE_BACK_BIT,
			renderer->window_target,
			OVERWRITE
		));
		vkDestroyShaderModule(renderer->device, triangleFragShader, nullptr);
		vkDestroyShaderModule(renderer->device, triangleVertexShader, nullptr);


		// Set up the postprocessor to handle lighting
		Variant gradient_shader_file_data = Variant::loadFileBytes("./shader/gradient_color.comp.spv");
		VkShaderModule gradientShader = renderer->loadShader(gradient_shader_file_data.getByteArray(), gradient_shader_file_data.getArrayLength());
		//std::vector< std::shared_ptr<VulkanImage>> screen_images = {window_color_image, window_normal_image, window_point_image };
		postprocess_shader =std::shared_ptr<ScreenShaderProgram>(new ScreenShaderProgram(renderer->device, gradientShader, sizeof(ComputePushConstants), renderer->window_target->images, 16));
		auto post_effect = std::shared_ptr<ScreenModel<ComputePushConstants, ComputeComponent>>(new ScreenModel<ComputePushConstants, ComputeComponent>(postprocess_shader));
		std::vector<ComputeComponent> components = { {glm::vec4(1,1,0,1)} };
		post_effect->setModel(components);
		post_effect->setConstantLocations(&post_effect->push_constants.world_matrix, &post_effect->push_constants.camera_position, &post_effect->push_constants.component_buffer);
		post_effect->phase = 1;
		post_effect->setTargets({renderer->window_target});
	/*
		if (OpenXRPlugin::ENABLED) {
			post_effect->targets.insert(xr->left_eye_target);
			post_effect->targets.insert(xr->right_eye_target);
		}
*/
		post_effect_id = renderer->addRenderable(post_effect);

		std::string file_path = "./assets/stone_cube_white.glb";
		meshes["box"] = VulkanDemoApp::loadGLTF(file_path, renderer, mesh_program);
		meshes["box"].second->transform = meshes["box"].second->getNormalizationTransform();

		/*
		int num_instances = 1;
		std::vector<GLTF::BufferInstance> mesh_instances = std::vector<GLTF::BufferInstance>(num_instances);
		glm::mat4 model_pose = glm::mat4(1.0f);
		model_pose = glm::scale(model_pose, glm::vec3(0.1, 0.1, 0.1));
		model_pose = glm::rotate(model_pose, 45.0f, glm::vec3(1, 0, 0));
		float d = 40.0f;
		for (int k = 0; k < num_instances; k++) {
			mesh_instances[k].root = glm::translate(model_pose, glm::vec3((randomFloat() * d) - d / 2, (randomFloat() * d) - d / 2, (randomFloat() * d) - d / 2));
			mesh_instances[k].root = glm::rotate(mesh_instances[k].root, 90.0f, glm::vec3(1, 0, 0));
			mesh_instances[k].root = glm::rotate(mesh_instances[k].root, randomFloat() * 6.28f, glm::vec3(0.0, 1, 0));
			for (int j = 0; j < 50; j++) {
				mesh_instances[k].bone_pose[j] = glm::mat4(1.0);
			}
		}
		for (int k = 0; k < meshes["fox"].first.size(); k++) {
			meshes["fox"].first[k]->setInstances(mesh_instances);
		}
		*/
		auto& fr = meshes["box"].first[0];
		fr->phase = 0;
		fr->group = 1;
		fr->setTargets({renderer->window_target});
		/*
		if (OpenXRPlugin::ENABLED) {
			fr->targets.insert(xr->left_eye_target);
			fr->targets.insert(xr->right_eye_target);
		}*/
		box_model_id = renderer->addRenderable(fr);

	}
	auto& br = meshes["box"].first[0];

	std::vector<GLTF::Instance256> instances =  br->instances ; // copy all imnstances
	
	glm::mat4 transform = glm::translate(glm::mat4(1), (min + max) * 0.5f);
	transform = glm::scale(transform, max - min);
	glm::mat4 scaled = glm::scale(transform, glm::vec3(0.5, 0.5, 0.5));

	GLTF::Instance256 new_instance;
	
	if(transform_group == host_transform_group){
		new_instance.root = host_transform * scaled;
	}else{
		new_instance.root = client_transform * scaled;
	}
	new_instance.bone_pose[0] = glm::mat4(1.0);

	int instance_id = (int)instances.size();
	
	instances.push_back(new_instance);
	br->setInstances(instances) ;
	//Variant(new_instance.root).printFormatted();
	

	return instance_id;

}


BallTestApp::Ball::Ball(const glm::vec3& p, const glm::vec3& v, float r, const glm::vec3& c, int64_t g) {
	position = p;
	velocity = v;
	radius = r;
	color = c;
	grid_id = g;

}

void BallTestApp::Ball::update() {

	
	position += velocity * tick_interval;

	std::shared_ptr<const WorldObject> r = read(grid_id);
	if(!r){
		return ;
	}

	std::shared_ptr<const Grid> grid = dynamic_pointer_cast<const Grid>(r);
	if (grid == nullptr) {
		
		return;
	}
	//Bounce inside the box
	if (position.x + radius > grid->max.x) {
		velocity.x *= -1.0f;
		position.x = grid->max.x - radius;
	}
	else if (position.x - radius < grid->min.x) {
		velocity.x *= -1.0f;
		position.x = grid->min.x + radius;
	}

	if (position.z + radius > grid->max.z) {
		velocity.z *= -1.0f;
		position.z = grid->max.z - radius;
	}
	else if (position.z - radius < grid->min.z) {
		velocity.z *= -1.0f;
		position.z = grid->min.z + radius;
	}
	
	//printf("cells size: %d\n", cells.size());
	std::vector<int64_t> new_cells = grid->getCells(position, radius);
	for (auto& new_cell : new_cells) {
		bool found = false;
		for (auto& old_cell : cells) {
			found |= old_cell == new_cell;
		}
		if (!found) { // new cell we didn't have before
			//printf("queueing add with %lld\n", id);
			queue(new_cell, time, &BallTestApp::Cell::addBall, id); // add us to it
		}
	}

	for (auto& old_cell : cells) {
		bool found = false;
		for (auto& new_cell : new_cells) {
			found |= old_cell == new_cell;
		}
		if (!found) { // old cell we aren't in anymore
			queue(old_cell, time, &BallTestApp::Cell::removeBall, id); // remove us from it
		}
	}
	cells = new_cells;
	//printf("new cells size: %d\n", new_cells.size());
	std::unordered_set<int64_t> checked; // don't check the same ball twice if we share two cells with it
	//printf("My ID: %lld \n", id);
	for (auto& cell_id : cells) {
		std::shared_ptr<const Cell> cell = dynamic_pointer_cast<const Cell>(read(cell_id));
		if (cell) {
			/*
			if (cell->writing_event) {
				if (time - cell->writing_event->actual_run_time <= Timeline::world->min_event_duration) {
					printf("disallowed read 3! Object writing event less tha min event duration\n");
				}
			}
			double t_distance = Timeline::preciseDistance(event_position, cell->position);
			if (t_distance / (time - cell->time) >= Timeline::world->max_info_speed) {
				printf("disallowed read 4! distance = %lf  time = %lf info_speed = %lf\n", t_distance, (time - cell->time), t_distance / (time - cell->time));
				cell->print();

				print();
			}*/

		
			//printf("cell balls size: %d\n", cell->balls.size());
			for (auto& other_id : cell->balls) {
				//printf("other_id: %lld\n", other_id);
				if (other_id > id && checked.find(other_id) == checked.end()) { // only do each collision once
					checked.insert(other_id);
					std::shared_ptr<const Ball> other = dynamic_pointer_cast<const Ball>(read(other_id));
					if (other) { // not necessarily a bug, since the cell logs the id before the ball is actually created
						//printf("other pos: %f,%f,%f\n", other->position.x, other->position.y, other->position.z);
						/*
						if(other->writing_event){
							if(time - other->writing_event->actual_run_time <= Timeline::world->min_event_duration){
								printf("disallowed read! Object writing event less tha min event duration\n") ;
							}
						}else{
							//printf("other does not have writing event!?\n") ;
						}
						double t_distance = Timeline::preciseDistance(event_position, other->position);
						if (t_distance / (time - other->time) >= Timeline::world->max_info_speed) {
							printf("disallowed read 2! distance = %lf  time = %lf info_speed = %lf\n", t_distance, (time - other->time), t_distance / (time - other->time));
							other->print();

							print();
						}
						*/
						glm::vec3 to_other = other->position - position;
						float distance = glm::length(to_other);
						
						

						if (distance < radius + other->radius && glm::dot(velocity - other->velocity, to_other) > 0) { // if in range and moving toward each other
							//printf("%lld, hit other  %lld at %lf\n", id, other->id, time);
							//impulse assumes mass is relative to area 'cause we don't have mass
							glm::vec3 impulse = to_other * (glm::dot((velocity * (radius * radius)) - (other->velocity * (other->radius * other->radius)), to_other) / glm::dot(to_other, to_other));
							if (player && other->player) {
								// Players just go through each other for now
							}else if (player) {
								queue(other_id, time, &BallTestApp::Ball::applyImpulse, impulse*2.0f); // pass the impulse of the collision onto othe other ball
							}
							else if (other->player) {
								velocity -= impulse*(2.0f / (radius * radius));
								velocity *= BallTestApp::ball_speed / glm::length(velocity); // fix velocity
							}else { // both not playurs
								velocity -= impulse / (radius * radius);
								velocity *= BallTestApp::ball_speed / glm::length(velocity); // fix velocity
								queue(other_id, time, &BallTestApp::Ball::applyImpulse, impulse); // pass the impulse of the collision onto othe other ball
							}
						}
					}
				}
			}
		}
	}

	queue(id, time + tick_interval, &BallTestApp::Ball::update);

	//printf("Ball update for %ld at %f (%f,%f,%f)\n", (long)id, time, position.x, position.y, position.z);
}
void BallTestApp::Ball::setVelocity(const glm::vec3& v) {
	//printf("Setting velocity: %f,%f,%f\n", v.x, v.y, v.z);
	velocity = v;
}
void BallTestApp::Ball::setPosition(const glm::vec3& p) {
	position = p;
}

void BallTestApp::Ball::applyImpulse(const glm::vec3& impulse) {
	velocity += impulse / (radius * radius);
	velocity *= BallTestApp::ball_speed / glm::length(velocity);// fix speed
}

void BallTestApp::Ball::print() const {
	printf("Ball at %f, %f, %f  v = %f, %f, %f\n", position.x, position.y, position.z, velocity.x, velocity.y, velocity.z);
}

BallTestApp::Cell::Cell(glm::vec3 mn, glm::vec3 mx) {
	//min = mn;
	//max = mx;
	position = (mn + mx) * 0.5f;
}

//Add or remove a reference to a ball in this cell
void BallTestApp::Cell::addBall(const int64_t& ball_id) {
	balls.insert(ball_id);
}
void BallTestApp::Cell::removeBall(const int64_t& ball_id) {
	balls.erase(ball_id) ;
}

void BallTestApp::Cell::print() const {
	printf("GridCell: %f, %f, %f\n" , position.x, position.y, position.z);
}

BallTestApp::Grid::Grid(glm::vec3 mn, glm::vec3 mx, int w, int h) {
	min = mn;
	max = mx;
	width = w;
	height = h;
	position = (min + max) * 0.5f;
}

// Build the grid
void BallTestApp::Grid::createGrid() {
	int k = 0; 
	for (int x = 0; x < width; x++) {
		
		float minx = min.x + (max.x - min.x) * x / (float)width;
		float maxx = min.x + (max.x - min.x) * (x + 1) / (float)width;
		std::vector<int64_t> row;
		for (int z = 0; z < height; z++) {
			float minz = min.z + (max.z - min.z) * z / (float)height;
			float maxz = min.z + (max.z - min.z) * (z + 1) / (float)height;
			glm::vec3 cellmin = glm::vec3(minx, min.y, minz);
			glm::vec3 cellmax = glm::vec3(maxx, max.y, maxz);
			auto cell = std::make_shared<Cell>(cellmin, cellmax);
			//printf("cell pos: %f, %f, %f\n", cell->position.x, cell->position.y, cell->position.z);
			int64_t cell_id = create(cell, time );
			row.push_back(cell_id);
			//printf("Making cell with id:%lld\n", cell_id);
		}
		grid.push_back(row);
	}
}

//Get the ids of the cells a ball intersects (not an event, the ball update event can call this beause it is const)
std::vector<int64_t> BallTestApp::Grid::getCells(const glm::vec3& center, float radius) const {
	std::unordered_set<int64_t> s;
	s.insert(getCell(glm::vec3(center.x - radius, center.y, center.z - radius)));
	s.insert(getCell(glm::vec3(center.x + radius, center.y, center.z - radius)));
	s.insert(getCell(glm::vec3(center.x - radius, center.y, center.z + radius)));
	s.insert(getCell(glm::vec3(center.x + radius, center.y, center.z + radius)));
	std::vector<int64_t> v;
	for (auto& i : s) {
		v.push_back(i);
	}
	return v;
}

//returns the cell a point is in
int64_t BallTestApp::Grid::getCell(const glm::vec3& p) const {
	int x = (int)((p.x - min.x)*width / (max.x - min.x));
	if (x < 0) x = 0;
	if (x > width - 1) x = width - 1;
	int z = (int)((p.z - min.z) * height / (max.z - min.z));
	if (z < 0) z = 0;
	if (z > height- 1) z = height - 1;
	return grid[x][z];
}

void BallTestApp::Grid::print() const {
	printf("Grid: min = %f,%f,%f  max= %f,%f,%f\n", min.x, min.y, min.z, max.x, max.y, max.z);
}


void BallTestApp::checkBaseObjects(double time) {
	//printf("Checking base objects at time: %lf\n", time);
	auto host_map = host_timeline.getBaseObjects(time);
	auto client_map = client_timeline.getBaseObjects(time);
	bool error = false;
	for(auto& [ id, obj] : host_map){
		if(client_map.find(id) == client_map.end()){
			printf("Object in host not in client:") ;
			obj->print();
			error= true;
		}else{
			std::vector<char> serial = host_timeline.serializeWorldObject(obj);
			int64_t host_hash = hashBytes(serial) ;
		
			std::vector<char> serial2 = client_timeline.serializeWorldObject(client_map[id]);
			int64_t client_hash = hashBytes(serial2);

			if(host_hash != client_hash){
				printf("Objects don't match\n");
				error = true ;
				obj->print();
				obj->writing_event->print() ;
				obj->writing_event->parent->print();
				client_map[id]->print();
				if(client_map[id]->writing_event){
					client_map[id]->writing_event->print();
					client_map[id]->writing_event->parent->print();
				}


			}else{
				/*
				printf("Objects match\n");
				obj->print();
				client_map[id]->print();
				*/
			}
		}
	}

	for (auto& [id, obj] : client_map) {
		if (host_map.find(id) == host_map.end()) {
			printf("Object in client not in host:");
			error = true ;
			obj->print();
		}
	}
	
	if (error) {

		throw std::runtime_error("Base Objects not in sync after sync, time windows might be too short for amount of warp.");
	}
	else {
		//printf("Check base objects passed\n");
	}
}