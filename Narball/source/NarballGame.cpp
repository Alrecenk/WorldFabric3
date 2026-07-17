#include "NarballGame.h"

#include "ScenePlugin.h"
#include "WorldPlugin.h"
#include "ParticlePlugin.h"
#include "AudioPlugin.h"
#include "Utilities.h"
#include "FlagSet.h"
#include "NarballMenu.h"
#include "StatePlugin.h"
#include "Ball.h"
#include "Nameplate.h"

#include <queue>

namespace Narball{

//Loads models from the hard drive on construction
NarballGame::NarballGame(){
	VulkanPlugin* window = getTool<VulkanPlugin>();
	if (window->window_height / (float)window->window_width > 9.0f / 16.0f) {
		fov *= window->window_height * 16.0f / (window->window_width * 9.0f); // if they have a taller screen, make sure they can see the whole field
	}
}

// Called when switching into this state before the first time run is claled
void NarballGame::enter(std::shared_ptr<MachineState> from) {
	WorldPlugin* worlds = getTool<WorldPlugin>();
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	AudioPlugin* sound = getTool<AudioPlugin>();

	printf("entering game\n");
	std::shared_ptr<NarballMenu> menu_state = dynamic_pointer_cast<NarballMenu>(from);
	
	// Grab the lobby pointer from the menu state
	lobby = worlds->observe<Lobby>(NARBALL, NarballMenu::lobby_id);
	if (!lobby) {
		printf("could not find lobby!?\n");
	}

	//Adjust the meta paramters based on the number of balls
	int grid_width = 32;
	int grid_height = 18 ;
	
	if(lobby->balls <= 20){
		tick_interval = 1.0f / 240.0f;
		ball_radius = 0.20f;
		grid_width = 16;
		grid_height = 9;
		wait_for_observables = true;
		wait_for_scene = true ;
		fancy_interpolation = true ;
	}else if (lobby->balls <= 100) {
		tick_interval = 1.0f / 120.0f;
		ball_radius = 0.20f;
		grid_width = 16;
		grid_height = 9 ;
		wait_for_observables = true;
		wait_for_scene = true;
		fancy_interpolation = true;
	}else if (lobby->balls <= 300) {
		ball_radius = 0.20f;
		tick_interval = 1.0f / 120.0f;
		wait_for_observables = false;
		wait_for_scene = false;
		fancy_interpolation = true;
	}else if(lobby->balls <= 600){
		ball_radius = 0.15f;
		tick_interval = 1.0f / 60.0f;
		wait_for_observables = false;
		wait_for_scene = false;
		fancy_interpolation = true;
	}else if(lobby->balls <= 1000){
		ball_radius = 0.11f;
		tick_interval = 1.0f / 60.0f;
		wait_for_observables = false;
		wait_for_scene = false;
		fancy_interpolation = false;
	}else{
		ball_radius = 0.1f;
		tick_interval = 1.0f / 60.0f;
		wait_for_observables = false;
		wait_for_scene = false;
		fancy_interpolation = false;
	}

	//Host initializes the field and balls
	if(worlds->amHosting()){
		double current_time = worlds->getWorldTime(NARBALL);
		printf("creating grid function: game time %lf\n", current_time) ;
		start_time = current_time + 3 ;
		auto grid = std::make_shared<Match>(lobby->id, glm::vec3(min_arena.x, -1, min_arena.y), glm::vec3(max_arena.x, 0, max_arena.y), grid_width, grid_height, tick_interval, start_time, ball_radius);
		match_id = worlds->create(NARBALL, grid);
		worlds->queue(NARBALL, match_id, &Match::createMatch);
		double ball_spawn_start = 0.2 ; // wait a bit to start spawning balls, so grid can initialize
		for (int k = 0; k < lobby->balls; k++) {
			auto ball = std::make_shared<Ball>(glm::vec3(-7 + randomFloat() * 14, 0, -4 + randomFloat() * 8), match_id);
			int64_t ball_id = worlds->create(NARBALL, ball, current_time+ball_spawn_start + 0.0005 * k); // create a bit in the future to give the grid a chance to initialize
			worlds->queue(NARBALL, ball_id, current_time +ball_spawn_start+0.1 + 0.0005 * k, &Ball::update); // 0.1 seconds later start the ball moving
		}

	}


	// update the camera, light, and scene for a match
	window->window_target->setCamera(camera_position, camera_look_at, fov, glm::vec3(0, 1, 0)); // fixed and facing the field
	glm::vec3 light_position = { 9.5,45,15.0f };
	scene->moveLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(0, light_position, camera_look_at, glm::vec3(0, 1, 0), 0.475f, 1000.0f);
	pool_instance = scene->createInstance(pool_model, glm::mat4(1.0));
	sound->SetListenerToHMD(window->window_target->camera_matrix);

	// Mute th3e music when entering a match, it starts after the countdown timer
	if (MUSIC_SOURCE < 0) {
		MUSIC_SOURCE = sound->createSource(glm::vec3(0, 0, 0));
		sound->setSourceGain(MUSIC_SOURCE, 1.0f);
	}else {
		sound->silence(MUSIC_SOURCE);
	}

	window->hideMouse();

	name_plates_visible = true ;

	last_keep_alive = now();
	last_control_time = now();


	printf("game entered\n");
}

void NarballGame::run(){
	// get all the tools we'll be using
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>();
	AudioPlugin* sound = getTool<AudioPlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	float dt =(float)( microsBetween(last_frame_time, now()) * 0.000001);
	last_frame_time = now();

	if (millisBetween(last_keep_alive, now()) > keep_alive_interval && !leaving && lobby) {
		worlds->queue(NARBALL, lobby->id, &Lobby::keepAlive, NarballMenu::local_player_id);
		last_keep_alive = now();
		if (worlds->amHosting()) {
			worlds->queue(NARBALL, lobby->id, &Lobby::kickDisconnected);
		}
	}


	//If trying to exit or got disconnected by the network or host
	if (window->keyDown(SDLK_ESCAPE) || window->getLastGamepadPress().second == SDL_GAMEPAD_BUTTON_BACK || !worlds->connected()) {
		if(worlds->amHosting()){ // if the server escapes
			//printf("host pressed exit\n");
			worlds->queue(NARBALL, match_id, &Match::closeMatch) ; // Deletes the match and sets the lobby back to open which should push everyone back to the menu
			next_state = NarballMenu::state_name;
		}else if(!leaving){ // if a client escapes
			//printf("client pressed exit\n");
			printf("exiting match\n");
			//worlds->queue(NARBALL, lobby_id, &Lobby::removePlayer, NarballMenu::local_player_id);
			leaving = true ;
			leave_time = now();	
		}
	}

	// Give the leave command a chance to get out before we disconnect
	if(leaving && millisBetween(leave_time, now())> 300){
		worlds->disconnect(); // leave the match
		worlds->clearWorlds(); // Wipe the data so we fall back to the main menu, not the lobby
		next_state = NarballMenu::state_name;
		leaving = false;
		return;
	}

	double vantage_time = worlds->getWorldTime(NARBALL); // time at vantage point


	NarballLightComponent light_component = scene->getLightComponent<ScenePlugin::ScreenPushConstants, NarballLightComponent>(0);
	light_component.time = (float)vantage_time;
	scene->setLightComponent<ScenePlugin::ScreenPushConstants, NarballLightComponent>(0, light_component);
	water_flow = light_component.flow_velocity ;

	NarwhalView::lobby = lobby; // Make lobby visible to narwhal views for nameplates
	NarwhalView::camera_position = camera_position;
	NarwhalView::nameplates_enabled = name_plates_visible;
	//worlds->view(NARBALL); // Call all linked view objects, handling most of the rendering (now handled by ViewPlugin)

	std::vector<std::shared_ptr<const WorldObject>> visible = worlds->observe(NARBALL);
	bool have_narwhal = false;
	bool have_player = false;
	int64_t my_narwhal = -1 ;
	// Find unique objects that the game logic needs to track
	
	for(int k=0;k<visible.size();k++){ //TODO stop looking for these items once they are found

		std::shared_ptr<const Narwhal> observed_narwhal = dynamic_pointer_cast<const Narwhal>(visible[k]) ;
		if(observed_narwhal && observed_narwhal->player_id == NarballMenu::local_player_id){
			my_narwhal = observed_narwhal->id ;
			have_narwhal = true ;
		}

		std::shared_ptr<const Match> grid = dynamic_pointer_cast<const Match>(visible[k]);
		if(grid){ // if it's a grid
			match_id = grid->id ; // grab the grid id, which allows us to make stuff with collision detection
			start_time = grid->start_time ;
			updateScoreDisplay(grid);
		}

		std::shared_ptr<const Lobby> maybe_lobby = dynamic_pointer_cast<const Lobby>(visible[k]);
		if (maybe_lobby) {
			lobby = maybe_lobby;
			if(lobby->state == Lobby::OPEN){
				next_state = NarballMenu::state_name ;
			}
			have_player = lobby->players.find(NarballMenu::local_player_id) != lobby->players.end() ;
		}
	}

	
	if(have_narwhal){
		std::shared_ptr<NarwhalView> local_narwhal_view = worlds->getView<NarwhalView>(NARBALL, my_narwhal);
		if(local_narwhal_view){
			local_narwhal_view->num_highlight = local_narwhal_highlight_particles ; //Put the highlight particles on the local narwhal's view
			worlds->setVantagePoint(NARBALL, local_narwhal_view->last_view.position); // updfate the local vantage point for time warp
		}
	}

	// Check if we need to ask for a player slot or a narwhal
	if((!have_player || !have_narwhal ) && millisBetween(last_create_request_time, now()) > keep_alive_interval){
		if(!have_player){
			printf("Adding player with name %s\n", NarballMenu::player_name->getString().c_str());
			worlds->queue(NARBALL, lobby->id, &Lobby::addPlayer, NarballMenu::local_player_id, NarballMenu::player_name->getString()); // add self to the player list
		}else if(!have_narwhal){
			std::shared_ptr<Narwhal> me = std::shared_ptr<Narwhal>(new Narwhal(glm::vec3(0, 0, 0), match_id, NarballMenu::local_player_id));
			if(!have_player){ // impossible if above is an else if after have player but in theory but you could join as neutral
				me->color = 0 ;
				me->position.x = (randomFloat() - 0.5f) * 2.0f;
				me->position.z = (randomFloat() - 0.5f) * 2.0f;
			}else if (lobby->players.at(NarballMenu::local_player_id).team) { // bool for if red team
				me->color = 1;
				me->position.x = 3.0f;
				me->facing_angle = 3.1415926f;
				me->position.z = (randomFloat() - 0.5f) * 6.0f;
			}else {
				me->color = 2;
				me->position.x = -3.0f;
				me->position.z = (randomFloat() - 0.5f) * 6.0f;
			}
			
			my_narwhal = worlds->create(NARBALL, me, worlds->getWorldTime(NARBALL) + 0.2); // create a bit in the future to give the grid a chance to initialize
			worlds->queue(NARBALL, my_narwhal, &Narwhal::update);//start the narwhal moving
			
			printf("Creating local narwhal %lf\n", worlds->getWorldTime(NARBALL));
		}
		last_create_request_time = now();
	}


	if (have_narwhal && !leaving) {// Control the local narwhal
		Uint32 which_pad = window->getLastGamepadPress().first; // select the gamepad that most recently pressed a button
		float left_x = -1.0f * window->getGamepadAxis(which_pad, SDL_GAMEPAD_AXIS_LEFTX);
		float left_y = -1.0f * window->getGamepadAxis(which_pad, SDL_GAMEPAD_AXIS_LEFTY);// game uses Vulkan coordinates which are flipped
		float right_x = -1.0f * window->getGamepadAxis(which_pad, SDL_GAMEPAD_AXIS_RIGHTX);
		float right_y = -1.0f * window->getGamepadAxis(which_pad, SDL_GAMEPAD_AXIS_RIGHTY);

		//TODO keyboard overrides should use rebindable controls
		left_x = window->keyDown(SDLK_a) ? 1.0f : left_x;
		left_x = window->keyDown(SDLK_d) ? -1.0f : left_x;
		left_y = window->keyDown(SDLK_w) ? 1.0f : left_y;
		left_y = window->keyDown(SDLK_s) ? -1.0f : left_y;
		right_x = window->keyDown(SDLK_LEFT) ? 1.0f : right_x;
		right_x = window->keyDown(SDLK_RIGHT) ? -1.0f : right_x;
		right_y = window->keyDown(SDLK_UP) ? 1.0f : right_y;
		right_y = window->keyDown(SDLK_DOWN) ? -1.0f : right_y;

		// Push controls into the world by queueing a void event
		glm::vec4 current_controls = { left_x, left_y, right_x, right_y };
		if (glm::distance(current_controls, last_controls) > 0.02 || millisBetween(last_control_time, now()) > keep_alive_interval) {
			double command_time = worlds->getWorldTime(NARBALL) + control_delay ;
			if(command_time != last_command_time){
				//AsyncPlugin::inputDisplay(VulkanPlugin::last_sdl_input_num, 1, false);
				worlds->queue(NARBALL, my_narwhal, command_time, &Narwhal::setControls, glm::vec2(left_x, left_y), glm::vec2(right_x, right_y), VulkanPlugin::last_sdl_input_num);
				last_command_time = command_time ;
				last_controls = current_controls;
				//printf("Control millis: %d\n", millisBetween(last_control_time, now())) ;
				last_control_time = now();
			}
		}

		int which_button = window->getLastGamepadPress().second;
		//only consider common face buttons so it doesn't trigger on analog stick press
		if (which_button == SDL_GAMEPAD_BUTTON_A || which_button == SDL_GAMEPAD_BUTTON_B || which_button == SDL_GAMEPAD_BUTTON_X || which_button == SDL_GAMEPAD_BUTTON_Y) {
			bool is_button_down = window->getGamepadButton(which_pad, which_button);
			if (is_button_down && !controller_button_held) {
				name_plates_visible = !name_plates_visible;
			}
			controller_button_held = is_button_down;
		}
		//can also toggle nameplates with space
		if (window->keyDown(SDLK_SPACE) && !space_held) {
			name_plates_visible = !name_plates_visible;
		}
		space_held = window->keyDown(SDLK_SPACE);

	}

	updateDebugPanel();

}

// Called when switching out of this state after the last time run is called
void NarballGame::exit(std::shared_ptr<MachineState> to){
	VulkanPlugin* window = getTool<VulkanPlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	scene->deleteInstance(pool_instance);

	for (int k = 0; k < red_score_particles.size(); k++) {
		particles->destroyParticle(red_score_particles[k]);
	}
	red_score_particles.clear();
	for (int k = 0; k < blue_score_particles.size(); k++) {
		particles->destroyParticle(blue_score_particles[k]);
	}
	blue_score_particles.clear();

	score_display.reset();
	new_display.reset();
	score_nameplates.clear();
	
	last_create_request_time = now(); //TODO make a bit earlier so it doesnt wait to request
	match_id = -1 ;
	lobby.reset();
	last_display_text = "SBGFIDD^FHBFODBDE?}~" ;
	worlds->setVantagePoint(NARBALL, glm::vec3(0,0,0));
	window->showMouse();
	printf("exited game\n");
}

void NarballGame::updateScoreDisplay(std::shared_ptr<const Match> match){

	ParticlePlugin* particles = getTool<ParticlePlugin>();
	AudioPlugin* sound = getTool<AudioPlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>() ;
	if (match->left_score < red_score_particles.size()) {
		for (int k = 0; k < red_score_particles.size(); k++) {
			particles->destroyParticle(red_score_particles[k]);
		}
		red_score_particles.clear();
	}
	if (match->right_score < blue_score_particles.size()) {
		for (int k = 0; k < blue_score_particles.size(); k++) {
			particles->destroyParticle(blue_score_particles[k]);
		}
		blue_score_particles.clear();
	}


	while (red_score_particles.size() < match->left_score && red_score_particles.size() < 200) {
		int p_id = particles->createParticle(0);
		particles->setColor(p_id, glm::vec4(1.0, randomFloat() * 0.2f, randomFloat() * 0.2f, 0.7f));
		glm::mat4 pose = glm::mat4(1.0f);
		int x = (int)(1 + red_score_particles.size() / 4);
		int z = (int)(red_score_particles.size() % 4);
		//printf("p_id: %d, x:%d, z: %d\n", p_id, x, z);
		pose = glm::translate(pose, glm::vec3(x * 0.26, 2.0f, 4.45f - z * 0.26));
		pose = glm::scale(pose, glm::vec3(0.1f));
		particles->setPose(p_id, pose);
		red_score_particles.push_back(p_id);
		//Variant(pose).printFormatted();
		sound->priorityPlay(SCORE_SOUND, glm::vec3(0, 0, -8), 1.0f, 4, 10);
	}
	while (blue_score_particles.size() < match->right_score && blue_score_particles.size() < 200) {
		int p_id = particles->createParticle(0);
		particles->setColor(p_id, glm::vec4(randomFloat() * 0.2f, randomFloat() * 0.2f, 1.0, 0.7f));
		glm::mat4 pose = glm::mat4(1.0f);
		int x = (int)(1 + blue_score_particles.size() / 4);
		int z = (int)(blue_score_particles.size() % 4);
		//printf("p_id: %d, x:%d, z: %d\n", p_id, x, z);
		pose = glm::translate(pose, glm::vec3(x * -0.26, 2.0f, 4.45f - z * 0.26));
		pose = glm::scale(pose, glm::vec3(0.1f));
		particles->setPose(p_id, pose);
		blue_score_particles.push_back(p_id);
		sound->priorityPlay(SCORE_SOUND, glm::vec3(0, 0, 8), 1.0f, 4, 10);
	}

	if(new_display){ // creating a display takes a frame, but deleting one is instant so we have to hold the old one for a frame to to avoid flashing when the score changes
		score_display = new_display  ;
		new_display.reset() ;
	}

	// wait a small amount before making labels as scores could still be in flux
	if(lobby->state == Lobby::ENDING && worlds->getWorldTime(NARBALL, lobby->position) - lobby->last_state_change_time > 0.15f){
		if (lobby->result != last_display_text) {
			last_display_text = lobby->result;
			new_display = std::shared_ptr<Nameplate>(new Nameplate(lobby->result, 0, "arial100"));
			new_display->size = 0.8f;
			new_display->update(end_result_position, camera_position, 1.0f);
			sound->play(ENDING_SOUND, glm::vec3(0, 0, 0));


			//quues to sort each team by score (first is score, second is player id)
			std::priority_queue<std::pair<int, int>> red_team;
			std::priority_queue<std::pair<int, int>> blue_team;

			for (const auto& [id, player] : lobby->players) {
				if (player.team) {
					red_team.push({player.score,id});
				}else {
					blue_team.push({ player.score,id });
				}
			}
		
			double red_x = 3 ;
			double blue_x = -3 ;
			double z = 2 ;
			double dz = -0.8 ;
			int i = 0 ;
			while(!red_team.empty()){
				auto [score, id] = red_team.top();
				red_team.pop();
				LobbyPlayer player = lobby->players.at(id) ;
				score_nameplates[id] = std::shared_ptr<Nameplate>(new Nameplate(concat(player.name + "   ", player.score), id, "arial75")) ;
				score_nameplates[id]->size = 0.35f ;
				score_nameplates[id]->update(glm::vec3(red_x, 0, z + dz * i), camera_position, 1.0);
				i++;
			}
			i = 0 ;
			while (!blue_team.empty()) {
				auto [score, id] = blue_team.top();
				blue_team.pop();
				LobbyPlayer player = lobby->players.at(id);
				score_nameplates[id] = std::shared_ptr<Nameplate>(new Nameplate(concat(player.name + "   ", player.score), id, "arial75"));
				score_nameplates[id]->size = 0.4f;
				score_nameplates[id]->update(glm::vec3(blue_x, 0, z + dz * i), camera_position, 1.0);
				i++;
			}


		}

	}else if(worlds->getWorldTime(NARBALL) < start_time){
		std::stringstream ss;
		ss << ceil(start_time - worlds->getWorldTime(NARBALL)) ;
		std::string new_display_text = ss.str();
		if (new_display_text != last_display_text) {
			last_display_text = new_display_text;
			score_display = std::shared_ptr<Nameplate>(new Nameplate(last_display_text, 0, "arial100"));
			score_display->size = 0.8f;
			score_display->update(countdown_position, camera_position, 1.0f);

			sound->play(COUNTDOWN_SOUND, glm::vec3(0,0,0) );
		}
		
	}else{
		std::stringstream ss;
		ss << "Red: " << match->left_score << "              Blue:" << match->right_score;
		std::string new_display_text = ss.str() ;
		if(new_display_text != last_display_text){
			last_display_text = new_display_text;

			//get the new display all set up before deleting the old display as it may not appear ofr a frame
			new_display = std::shared_ptr<Nameplate>(new Nameplate(last_display_text, 0, "arial75"));
			new_display->size = 0.4f;
			if (name_plates_visible) {
				new_display->update(score_position, camera_position, 1.0f);
			}else {
				new_display->update(camera_position - camera_look_at * 2.0f, camera_position, 1.0f); // behind the camera
			}
			
			if(!score_display){
				score_display = new_display ;
			}
			
		}
		if(name_plates_visible){
			score_display->update(score_position, camera_position, 1.0f);
		}else{
			score_display->update(camera_position - camera_look_at * 2.0f, camera_position, 1.0f); // behind the camera
		}

		//Queue the music on score update so it won't start until the countdonw ends
		if (sound->amountQueued(MUSIC_SOURCE) < 2) {
			sound->queueSound(MATCH_MUSIC, MUSIC_SOURCE);
		}
	}

}


} // end namespace Narball