#include "NarballMenu.h"
#include "NarballGame.h"
#include "ParticlePlugin.h"
#include "ScenePlugin.h"
#include "Variant.h"
#include "FlagSet.h"
#include "Registry.h"
#include "PanelPlugin.h"
#include "AudioPlugin.h"
#include "StatePlugin.h"
#include "SteamworksPlugin.h"

namespace Narball{

NarballMenu::NarballMenu(){
	local_player_id = (int)(randomFloat() * 1E9);
}

// Called when switching into this state before the first time run is called
void NarballMenu::enter(std::shared_ptr<MachineState> from){
	printf("entering menu\n");
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	OpenXRPlugin* xr = getTool<OpenXRPlugin>();
	AudioPlugin* sound = getTool<AudioPlugin>();
	PanelPlugin* panels = getTool<PanelPlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>();
	SteamworksPlugin* steam = getTool<SteamworksPlugin>();
	steam->setSteamEventReceiver(this);
	
	//position the beach for the background
	glm::mat4 beach_pose = glm::mat4(1.0f);
	beach_pose = glm::translate(beach_pose, glm::vec3(-4.0, 1.5f, 0.5));
	beach_pose = glm::rotate(beach_pose, 3.141f*0.5f, glm::vec3(0,1, 0));
	beach_pose = glm::rotate(beach_pose, 3.141f * 0.1f, glm::vec3(1, 0, 0));
	beach_pose = glm::scale(beach_pose, glm::vec3(4, 4, 4));
	beach_id = scene->createInstance(beach_model, beach_pose);


	// place the light
	glm::vec3 light_position = { 15,25,5.0f };
	glm::vec3 light_look_at = glm::vec3(0, 0, 0);
	scene->moveLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(0, light_position, light_look_at, glm::vec3(0, 1, 0), 0.5f, 3000.0f);

	
	// update the camera
	window->window_target->setCamera(camera_position, camera_look_at, fov, glm::vec3(0, 1, 0));
	sound->SetListenerToHMD(window->window_target->camera_matrix);

	

	if(buttons.size() == 0){

		createMenus();
	}else{
		if(worlds->connected()){
			showLeft(lobby_menu) ;
		}else{
			showLeft(main_menu) ;
		}
	}

	// Start the righ tmusic when entering the menu
	if(MUSIC_SOURCE < 0){
		MUSIC_SOURCE = sound->createSource(glm::vec3(0,0,0)) ;
		sound->setSourceGain(MUSIC_SOURCE,1.0f) ;
	}else{
		sound->silence(MUSIC_SOURCE);
	}
	sound->queueSound(MENU_MUSIC, MUSIC_SOURCE);

	lobby_id = -1; // reset lobby id so we don't read old data after leaving a match
	
}


void NarballMenu::run() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	PanelPlugin* panels = getTool<PanelPlugin>();
	AudioPlugin* sound = getTool<AudioPlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>();
	SteamworksPlugin* steam = getTool<SteamworksPlugin>();
	
	// Keepthe music looping smoothly
	if(sound->amountQueued(MUSIC_SOURCE) < 2){
		sound->queueSound(MENU_MUSIC, MUSIC_SOURCE);
	}
	

	//handle the mouse interacting with the menu
	glm::vec3 mouse_ray = window->getMouseRay();
	panels->setPointerByRay(window->window_target->camera_position, mouse_ray);
	std::map<int, bool> mouse_down;
	for(int k = 0; k < 5; k ++){ // First five buttons because some people have weird mice
		mouse_down[k] = window->mouseDown(k);
		if(mouse_down[k] && !last_mouse_down[k]){
			panels->pressPointer(k + MOUSE_BUTTON_OFFSET);
		}
	}
	last_mouse_down = mouse_down ;

	//handle the controller intertacting with the menu

	Uint32 which_pad = window->getLastGamepadPress().first; // select the gamepad that most recently pressed a button
	float left_x = 1.0f * window->getGamepadAxis(which_pad, SDL_GAMEPAD_AXIS_LEFTX);
	float left_y = -1.0f * window->getGamepadAxis(which_pad, SDL_GAMEPAD_AXIS_LEFTY);
	float right_x = 1.0f * window->getGamepadAxis(which_pad, SDL_GAMEPAD_AXIS_RIGHTX);
	float right_y = -1.0f * window->getGamepadAxis(which_pad, SDL_GAMEPAD_AXIS_RIGHTY);

	//TODO keyboard overrides should use rebindable controls
	left_x = window->keyDown(SDLK_a) || window->keyDown(SDLK_LEFT) ? -1.0f : left_x;
	left_x = window->keyDown(SDLK_d) || window->keyDown(SDLK_RIGHT) ? 1.0f : left_x;
	left_y = window->keyDown(SDLK_w) || window->keyDown(SDLK_UP) ? 1.0f : left_y;
	left_y = window->keyDown(SDLK_s) || window->keyDown(SDLK_DOWN) ? -1.0f : left_y;
	
	glm::vec2 current_controls = { left_x, left_y };
	if (glm::length(current_controls) > 0.4 && millisBetween(last_control_time, now()) > 200) {
		if(panels->getNavSelection().first != active_menu){
			panels->setNavSelection(active_menu, start_nav_select[active_menu]);
		}
		panels->moveNavSelection(current_controls) ;
		last_controls = current_controls;
		last_control_time = now();
	}

	int which_button = window->getLastGamepadPress().second;
	bool trying_to_exit = false;
	if (which_button == SDL_GAMEPAD_BUTTON_A || which_button == SDL_GAMEPAD_BUTTON_X) {
		bool is_button_down = window->getGamepadButton(which_pad, which_button);
		if (is_button_down && !controller_button_held) {
			if(which_pad == last_active_pad){ // don't press on the same command that woke up the controller
				panels->pressNav(which_button) ;
			}
		}
		if(controller_button_held && !is_button_down){
			panels->releaseNav(which_button);
		}
		controller_button_held = is_button_down;
	}
	if (which_button == SDL_GAMEPAD_BUTTON_B ||  which_button == SDL_GAMEPAD_BUTTON_Y || which_button == SDL_GAMEPAD_BUTTON_BACK) {
		bool is_button_down = window->getGamepadButton(which_pad, which_button);
		if (is_button_down && !controller_button_held) {
			trying_to_exit = true ;
		}
	}
	if(which_pad != last_active_pad && which_pad >= 0){
		panels->setNavSelection(active_menu, start_nav_select[active_menu]);
	}


	last_active_pad = which_pad ;
	//can also press with space bar
	if ((window->keyDown(SDLK_SPACE) || window->keyDown(SDLK_RETURN)) && !space_held) {
		panels->pressNav(SDLK_SPACE);
	}
	space_held = window->keyDown(SDLK_SPACE) || window->keyDown(SDLK_RETURN) ;

	//Update the flowing water in the background
	NarballLightComponent light_component = scene->getLightComponent<ScenePlugin::ScreenPushConstants, NarballLightComponent>(0);
	light_component.time = (float)panels->getTime();
	scene->setLightComponent<ScenePlugin::ScreenPushConstants, NarballLightComponent>(0, light_component);

	// Allow typing into a selected text box
	if(selected_text_box != nullptr){
		selected_text_box->update();
	}else if (hovered_text_box != nullptr) {
		hovered_text_box->update();
	}

	if (waiting_on_join && worlds->connected()) { // pending connection just succeeeded
		waiting_on_join = false;
		joined = true;
		hosting = false;
		has_player = false;
		showRight(lobby_menu);
	}else if(waiting_on_join && millisBetween(join_start,now()) > 3000){// pending connection has timed out
		printf("Timed out trying to join.\n");
		waiting_on_join = false ;
		worlds->disconnect(); // clean it up in case it's still trying
		worlds->clearWorlds();
		steam->disconnect();
	}else if(waiting_on_join){ // join is pending
	
	}else if(trying_to_quickplay && steam->serverListReady()){
		std::vector<SteamworksPlugin::SteamServerInfo> servers = steam->getServerList();
		
		trying_to_quickplay = false;
		int best_score = 999999 ;
		int best_index  = -1 ;
		
		player_count_offset[0] = 100 ;
		player_count_offset[1] = 50 ;
		player_count_offset[2] = 50;
		player_count_offset[3] = 50;
		for(int k=0;k<servers.size();k++){
			if(servers[k].players < servers[k].max_players){
				int score = servers[k].ping ;
				if(servers[k].players < player_count_offset.size()){
					score += player_count_offset[servers[k].players] ;
				}else{
					score += player_count_offset[player_count_offset.size()-1] ;
				}
				if(score < best_score){
					best_score = score ;
					best_index = k ;
				}
			}
		}

		if(best_index == -1){
			printf("No dedicated servers found :( . You can host one with DedicatedServer.exe or use the host and join options in game for P2P play with friends.\n"); 
			waiting_on_join = false;
		}else{
			printf("Server selected. Score: %d  Attempting to join: %s\n", best_score, servers[best_index].name.c_str()) ;
			printf("Connect: %s\n", servers[best_index].connect.c_str());
			waiting_on_join = true;
			join_start = now();
			SteamNetworkingIPAddr addr ;
			addr.ParseString(servers[best_index].connect.c_str()) ;
			steam->joinAddress(addr) ;
		}

	}

	// Spin the quick join button while waiting to join
	if(trying_to_quickplay || waiting_on_join){
		quick_join_button_spinning = true ;
		//spin the join button while waiting
		glm::mat4 throb_pose = quick_join_button->hover_pose;
		throb_pose = throb_pose * glm::translate(glm::mat4(1.0),glm::vec3(0, 0.5, 0)) * glm::rotate(glm::mat4(1.0), (float)panels->getTime() * 3.0f, glm::vec3(1, 0, 0)) * glm::translate(glm::mat4(1.0), glm::vec3(0, -0.5, 0));
		panels->clearKeyFrames(quick_join_button->panel, quick_join_button->element);
		PanelPlugin::DefaultInstance inst = panels->getPanelElementInstance<PanelPlugin::DefaultInstance>(quick_join_button->panel, quick_join_button->element);
		inst.pose = throb_pose;
		panels->addPanelElementKeyFrame(quick_join_button->panel, quick_join_button->element, panels->getTime(), inst);
	}
	if(quick_join_button_spinning && !trying_to_quickplay && !waiting_on_join){
		quick_join_button_spinning = false ;
		// Stop the join button spin
		PanelPlugin::DefaultInstance inst = panels->getPanelElementInstance<PanelPlugin::DefaultInstance>(quick_join_button->panel, quick_join_button->element);
		panels->clearKeyFrames(quick_join_button->panel, quick_join_button->element);
		inst.pose = quick_join_button->base_pose;
		panels->addPanelElementKeyFrame(quick_join_button->panel, quick_join_button->element, panels->getTime(), inst);
	}

	// If we've lost connection to the host then leave the match
	if(joined && !leaving && !worlds->connected()){
		leaving = true ;
		leave_time = now();
	}

	//Actually disconnect a short time after leaving to give the leave action packet a chance to get through
	if(leaving && millisBetween(leave_time, now()) > 200){
		worlds->disconnect();
		worlds->clearWorlds();
		steam->disconnect();

		//Go back to the menu we came from (might already be there if left on purpose)
		showLeft(main_menu);

		leaving = false;
		hosting = false;
		joined = false;
		has_player = false ;
		player_label.clear();
		
	}

	if (!leaving) {
		
			//double observation_time = worlds->getWorldTime(NARBALL, visible[k]->position);
			//float observation_age = (float)(observation_time - visible[k]->time);

		std::shared_ptr<const Lobby> lobby = worlds->observeNearest<Lobby>(NARBALL);
		if (lobby) {// if the lobby is available
			lobby_id = lobby->id;
			//printf("lobby_id set to %lld\n", lobby_id);
			if (!has_player && lobby->state == Lobby::OPEN) { // we're a client connecting and receiving the lobby for the first time
				has_player = true;
				worlds->queue(NARBALL, lobby_id, &Lobby::addPlayer, local_player_id, player_name->getString()); // add self to the player list
				last_keep_alive = now();
			}

			// Update the lobby menu with the synced lobby data
			if (lobby->state != Lobby::CLOSED) {
				updateLobbyMenu(lobby);
			}

			//Lobby has started the match
			if (lobby->state == Lobby::STARTED && millisInState() > 300) { // small delay prevents bouncing between states if there's rollback
				//Switch the main app state
				StatePlugin* app = getTool<StatePlugin>();
				lobby->print();
				printf("Setting state to start match\n");
				next_state = NarballGame::state_name;
			}

			//Send periodic keep alive packets so the server knows we're still in
			if (millisBetween(last_keep_alive, now()) > keep_alive_interval) {
				worlds->queue(NARBALL, lobby_id, &Lobby::keepAlive, local_player_id);
				last_keep_alive = now();
				if (hosting) {
					worlds->queue(NARBALL, lobby_id, &Lobby::kickDisconnected);
				}
			}

			// Lobby says it's closed, so leave
			if (has_player && joined && lobby->state == Lobby::CLOSED) {
				printf("Leaving because lobby was closed.\n");
				worlds->queue(NARBALL, lobby_id, &Lobby::removePlayer, local_player_id);
				leaving = true;
				leave_time = now();
				has_player = false;
				showLeft(main_menu);

			}

		}

	}

	// Check if escape pressed to exit
	trying_to_exit |= window->keyDown(SDLK_ESCAPE) ;
	if (trying_to_exit && millisInState() > 500) {
		if(active_menu == lobby_menu){
			//Do the same thing as pressing the leave button
			if (joined) {
				if (has_player) {
					worlds->queue(NARBALL, lobby_id, &Lobby::removePlayer, local_player_id);
				}
				leaving = true; // don't disconnect immediately to give the signal that we left a chance to get through
				showLeft(main_menu);
			}
			if (hosting) {
				worlds->queue(NARBALL, lobby_id, &Lobby::setState, Lobby::CLOSED);
				leaving = true; // don't disconnect immediately to give the signal that we left a chance to get through
				showLeft(main_menu);
			}
			leave_time = now();
		}else if(!leaving){
			showLeft(main_menu) ;
		}
		//getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
	}
	
	
	updateDebugPanel();

}


// Called when switching outof this state after the last time run is called
void NarballMenu::exit(std::shared_ptr<MachineState> to){
	ScenePlugin* scene = getTool<ScenePlugin>();
	scene->deleteInstance(beach_id) ;
	hideLeft(active_menu);
	controller_button_held = false ;
	printf("exited menu\n");
}

int64_t NarballMenu::getLobbyID(){
	return lobby_id ;
}



void NarballMenu::createMenus(){
	VulkanPlugin* window = getTool<VulkanPlugin>();
	PanelPlugin* panels = getTool<PanelPlugin>();
	SteamworksPlugin* steam = getTool<SteamworksPlugin>() ;
	default_name = steam->getLocalName();
	printf("Entered Menu\n");
	left_menu_hide.pose = panels->getPoseInFrontOfCamera(camera_position, camera_look_at, 0.5f, 0.001f, (0.1f * menu_height) / menu_width);
	left_menu_hide.pose = glm::translate(glm::mat4(1), glm::vec3(0.03f, 0, 0)) * left_menu_hide.pose;
	menu_visible.pose = panels->getPoseInFrontOfCamera(camera_position, camera_look_at, 0.5f, 0.1f, (0.1f * menu_height) / menu_width);
	menu_visible.pose = glm::translate(glm::mat4(1), glm::vec3(0.09f, 0, 0)) * menu_visible.pose;
	right_menu_hide.pose = panels->getPoseInFrontOfCamera(camera_position, camera_look_at, 0.5f, 0.001f, (0.1f * menu_height) / menu_width);
	right_menu_hide.pose = glm::translate(glm::mat4(1), glm::vec3(0.17f, 0.0f, 0.0f)) * right_menu_hide.pose;
	menu_hidden.pose = PanelPlugin::HIDDEN ;

	createMainMenu();
	createOptionsMenu();
	createLobbyMenu();
	createCreditsMenu() ;

	
	//Catch menu actions
	panels->setListener(this);
	showLeft(main_menu) ;
}

void NarballMenu::createMainMenu(){
	PanelPlugin* panels = getTool<PanelPlugin>();
	VulkanPlugin* window = getTool<VulkanPlugin>();

	main_menu = panels->createPanel(menu_width, menu_height, { 0.5,0.5,0.5,0.5 });
	
	int title_element = panels->createElement(main_menu);
	std::shared_ptr<WFImage> title_texture = window->loadImageFromFile("./Narball/asset/NarballTitle.png");
	panels->setElementTexture(main_menu, title_element, title_texture);
	panels->setElementPosition(main_menu, title_element, glm::vec2(0, 10), (float)title_texture->getWidth(), (float)title_texture->getHeight());
	
	float text_center_x = menu_width / 2.0f;
	float text_start_y = 400.0f;
	float y_step = 150.0f;
	std::vector<std::string> button_text = { "Quick Play", "Join", "Private Host", "Options", "Exit" };
	
	int k = 0;
	std::vector<int> button_elements ;
	for (auto& text : button_text) {
		std::shared_ptr<PanelPlugin::MenuButton> button = std::shared_ptr<PanelPlugin::MenuButton>( new PanelPlugin::MenuButton(main_menu, text, text, text_center_x, text_start_y + k * y_step, true, "arial100"));
		buttons[std::pair<int, int>(main_menu, button->element)] = button;
		if (k == 0) {
			quick_join_button = button;
		}
		else if (k == 1) {
			join_button = button;
		}

		k++;
		button_elements.push_back(button->element) ;
		
	}

	for(int k=0;k<button_elements.size();k++){
		//up and down with wrapping
		panels->createNavLink(main_menu, button_elements[k], glm::vec2(0,-1), main_menu, button_elements[(k+1)%button_elements.size()]) ;
		panels->createNavLink(main_menu, button_elements[(k + 1) % button_elements.size()], glm::vec2(0, 1), main_menu, button_elements[k]);
		//side to side stays in place
		panels->createNavLink(main_menu, button_elements[k], glm::vec2(1, 0), main_menu, button_elements[k]);
		panels->createNavLink(main_menu, button_elements[k], glm::vec2(-1, 0), main_menu, button_elements[k]);
	}
	start_nav_select[main_menu] = button_elements[0] ;
}
void NarballMenu::createOptionsMenu(){
	PanelPlugin* panels = getTool<PanelPlugin>();
	VulkanPlugin* window = getTool<VulkanPlugin>();

	options_menu = panels->createPanel(menu_width, menu_height, { 0.8,0.8,1,0.5 });


	options_title = std::shared_ptr<PanelPlugin::Label>(new PanelPlugin::Label(options_menu, "Options", menu_width * 0.5f, 10, true, "arial100"));
	options_title->setColors(glm::vec4(.5, .0, .4, 0.4), glm::vec4(.5, 0, .4, 0.6), glm::vec4(1, 0.5, 0.15, 1), glm::vec4(0, 0, 0, 0));
	//sound options

	float music_volume_height = 300.0f ;
	music_volume_label = std::shared_ptr<PanelPlugin::Label>(new PanelPlugin::Label(options_menu, "Music Volume:", menu_width * 0.3f, music_volume_height, true, "arial75"));
	music_volume_label->setColors(glm::vec4(1,1,1,0.0), glm::vec4(1,1,1,0.0), glm::vec4(0, 0, 0, 1), glm::vec4(1,1,1,0)) ;
	auto music_volume_down = std::shared_ptr<PanelPlugin::MenuButton>(new PanelPlugin::MenuButton(options_menu, "<", "music_down",menu_width * 0.65f, music_volume_height, true, "arial75"));
	buttons[std::pair<int, int>(options_menu, music_volume_down->element)] = music_volume_down ;
	music_volume_number = std::shared_ptr<PanelPlugin::Label>(new PanelPlugin::Label(options_menu, concat("", music_volume), menu_width * 0.75f, music_volume_height, true, "arial75"));
	music_volume_number->setColors(glm::vec4(1, 1, 1, 0.0), glm::vec4(1, 1, 1, 0.0), glm::vec4(0, 0, 0, 1), glm::vec4(1, 1, 1, 0));
	auto music_volume_up = std::shared_ptr<PanelPlugin::MenuButton>(new PanelPlugin::MenuButton(options_menu, ">", "music_up", menu_width * 0.85f, music_volume_height, true, "arial75"));
	buttons[std::pair<int, int>(options_menu, music_volume_up->element)] = music_volume_up;
	
	float effects_volume_height = 400.0f;
	effects_volume_label = std::shared_ptr<PanelPlugin::Label>(new PanelPlugin::Label(options_menu, "Effects Volume:", menu_width * 0.3f, effects_volume_height, true, "arial75"));
	effects_volume_label->setColors(glm::vec4(1, 1, 1, 0.0), glm::vec4(1, 1, 1, 0.0), glm::vec4(0, 0, 0, 1), glm::vec4(1, 1, 1, 0));
	auto effects_volume_down = std::shared_ptr<PanelPlugin::MenuButton>(new PanelPlugin::MenuButton(options_menu, "<", "effects_down", menu_width * 0.65f, effects_volume_height, true, "arial75"));
	buttons[std::pair<int, int>(options_menu, effects_volume_down->element)] = effects_volume_down;
	effects_volume_number = std::shared_ptr<PanelPlugin::Label>(new PanelPlugin::Label(options_menu, concat("",effects_volume), menu_width * 0.75f, effects_volume_height, true, "arial75"));
	effects_volume_number->setColors(glm::vec4(1, 1, 1, 0.0), glm::vec4(1, 1, 1, 0.0), glm::vec4(0, 0, 0, 1), glm::vec4(1, 1, 1, 0));
	auto effects_volume_up = std::shared_ptr<PanelPlugin::MenuButton>(new PanelPlugin::MenuButton(options_menu, ">", "effects_up", menu_width * 0.85f, effects_volume_height, true, "arial75"));
	buttons[std::pair<int, int>(options_menu, effects_volume_up->element)] = effects_volume_up;
	
	
	player_name = std::shared_ptr<PanelPlugin::TextBox>(new PanelPlugin::TextBox(options_menu, "Name:", default_name, "name", menu_width * 0.25f, 500, "arial75", 12, false));
	text_boxes[std::pair<int, int>(options_menu, player_name->box_element)] = player_name;
	text_boxes[std::pair<int, int>(options_menu, player_name->text_element)] = player_name;

	
	std::shared_ptr<PanelPlugin::MenuButton> toggle_debug = std::shared_ptr<PanelPlugin::MenuButton>(new PanelPlugin::MenuButton(options_menu, "Toggle Debug Info", "toggle_debug", menu_width * 0.5f, 600, true, "arial75"));
	buttons[std::pair<int, int>(options_menu, toggle_debug->element)] = toggle_debug;

	toggle_fps = std::shared_ptr<PanelPlugin::MenuButton>(new PanelPlugin::MenuButton(options_menu, "Toggle Vsync", "toggle_fps", menu_width * 0.5f, 700, true, "arial75"));
	buttons[std::pair<int, int>(options_menu, toggle_fps->element)] = toggle_fps;

	std::shared_ptr<PanelPlugin::MenuButton> credits = std::shared_ptr<PanelPlugin::MenuButton>(new PanelPlugin::MenuButton(options_menu, "Credits", "credits", menu_width * 0.5f, 800, true, "arial75"));
	buttons[std::pair<int, int>(options_menu, credits->element)] = credits;

	std::shared_ptr<PanelPlugin::MenuButton> back = std::shared_ptr<PanelPlugin::MenuButton>(new PanelPlugin::MenuButton(options_menu, "Back", "Back", menu_width / 2.0f, 950, true, "arial100"));
	buttons[std::pair<int, int>(options_menu, back->element)] = back;
	

	glm::vec2 up = glm::vec2(0, 1) ;
	glm::vec2 down = glm::vec2(0, -1);
	glm::vec2 left = glm::vec2(-1, 0);
	glm::vec2 right = glm::vec2(1, 0);

	panels->createNavLink(options_menu, music_volume_down->element, right, options_menu, music_volume_up->element) ;
	panels->createNavLink(options_menu, music_volume_down->element, down, options_menu, effects_volume_down->element);
	panels->createNavLink(options_menu, music_volume_down->element, left, options_menu, music_volume_down->element);
	panels->createNavLink(options_menu, music_volume_down->element, up, options_menu, back->element);

	panels->createNavLink(options_menu, music_volume_up->element, right, options_menu, music_volume_up->element);
	panels->createNavLink(options_menu, music_volume_up->element, down, options_menu, effects_volume_up->element);
	panels->createNavLink(options_menu, music_volume_up->element, left, options_menu, music_volume_down->element);
	panels->createNavLink(options_menu, music_volume_up->element, up, options_menu, back->element);

	panels->createNavLink(options_menu, effects_volume_down->element, right, options_menu, effects_volume_up->element);
	panels->createNavLink(options_menu, effects_volume_down->element, down, options_menu, toggle_debug->element);
	panels->createNavLink(options_menu, effects_volume_down->element, left, options_menu, effects_volume_down->element);
	panels->createNavLink(options_menu, effects_volume_down->element, up, options_menu, music_volume_down->element);

	panels->createNavLink(options_menu, effects_volume_up->element, right, options_menu, effects_volume_up->element);
	panels->createNavLink(options_menu, effects_volume_up->element, down, options_menu, toggle_debug->element);
	panels->createNavLink(options_menu, effects_volume_up->element, left, options_menu, effects_volume_down->element);
	panels->createNavLink(options_menu, effects_volume_up->element, up, options_menu, music_volume_up->element);

	panels->createNavLink(options_menu, toggle_debug->element, right, options_menu, toggle_debug->element);
	panels->createNavLink(options_menu, toggle_debug->element, down, options_menu, toggle_fps->element);
	panels->createNavLink(options_menu, toggle_debug->element, left, options_menu, toggle_debug->element);
	panels->createNavLink(options_menu, toggle_debug->element, up, options_menu, effects_volume_down->element);

	panels->createNavLink(options_menu, toggle_fps->element, right, options_menu, toggle_fps->element);
	panels->createNavLink(options_menu, toggle_fps->element, down, options_menu, credits->element);
	panels->createNavLink(options_menu, toggle_fps->element, left, options_menu, toggle_fps->element);
	panels->createNavLink(options_menu, toggle_fps->element, up, options_menu, toggle_debug->element);

	panels->createNavLink(options_menu, credits->element, right, options_menu, credits->element);
	panels->createNavLink(options_menu, credits->element, down, options_menu, back->element);
	panels->createNavLink(options_menu, credits->element, left, options_menu, credits->element);
	panels->createNavLink(options_menu, credits->element, up, options_menu, toggle_fps->element);

	panels->createNavLink(options_menu, back->element, right, options_menu, back->element);
	panels->createNavLink(options_menu, back->element, up, options_menu, credits->element);
	panels->createNavLink(options_menu, back->element, left, options_menu, back->element);
	panels->createNavLink(options_menu, back->element, down, options_menu, music_volume_down->element);

	start_nav_select[options_menu] = music_volume_down->element;

}

void NarballMenu::createCreditsMenu() {
	PanelPlugin* panels = getTool<PanelPlugin>();
	VulkanPlugin* window = getTool<VulkanPlugin>();

	credits_menu = panels->createPanel(menu_width, menu_height, { 0.8,0.8,1,0.5 });

	credit_labels.clear();

	auto credits_title = std::shared_ptr<PanelPlugin::Label>(new PanelPlugin::Label(credits_menu, "Credits", menu_width * 0.5f, 10, true, "arial100"));
	credits_title->setColors(glm::vec4(.5, .0, .4, 0.4), glm::vec4(.5, 0, .4, 0.6), glm::vec4(1, 0.5, 0.15, 1), glm::vec4(0, 0, 0, 0));
	credit_labels.push_back(credits_title) ;
	
	std::vector<std::string> credits ={ 
		"Narball created by Alrecenk", 
		"Powered by World Fabric",
		"Music by Zane Little",
		"Trailer by Jordon Olson" 
	} ;


	float y = 120;
	float y_step = 100;
	for(std::string& line : credits ){
		auto credit = std::shared_ptr<PanelPlugin::Label>(new PanelPlugin::Label(credits_menu, line, menu_width * 0.5f, y, true, "arial75"));
		credit->setColors(glm::vec4(1, 1, 1, 0.0), glm::vec4(1, 1, 1, 0.0), glm::vec4(0, 0, 0, 1), glm::vec4(0, 0, 0, 0));
		credit_labels.push_back(credit);
		y+=y_step ;
	}

	std::shared_ptr<PanelPlugin::MenuButton> back = std::shared_ptr<PanelPlugin::MenuButton>(new PanelPlugin::MenuButton(credits_menu, "Back", "Back", menu_width / 2.0f, 950, true, "arial100"));
	buttons[std::pair<int, int>(credits_menu, back->element)] = back;


	start_nav_select[credits_menu] = back->element;
}


void NarballMenu::createLobbyMenu(){
	PanelPlugin* panels = getTool<PanelPlugin>();
	VulkanPlugin* window = getTool<VulkanPlugin>();

	lobby_menu = panels->createPanel(menu_width, menu_height, { 0.8,0.8,1,0.5 });




	lobby_title = std::shared_ptr<PanelPlugin::Label>(new PanelPlugin::Label(lobby_menu, "Lobby", menu_width * 0.5f, 10, true, "arial100"));
	lobby_title->setColors(glm::vec4(.5, .0, .4, 0.4), glm::vec4(.5, 0, .4, 0.6), glm::vec4(1, 0.5, 0.15, 1), glm::vec4(0, 0, 0, 0));


	lobby_balls = std::shared_ptr<PanelPlugin::TextBox>(new PanelPlugin::TextBox(lobby_menu, "Balls:", "2", "balls", menu_width * 0.5f, 140, "arial75", 5, true));
	text_boxes[std::pair<int, int>(lobby_menu, lobby_balls->box_element)] = lobby_balls;
	text_boxes[std::pair<int, int>(lobby_menu, lobby_balls->text_element)] = lobby_balls;

	lobby_points = std::shared_ptr<PanelPlugin::TextBox>(new PanelPlugin::TextBox(lobby_menu, "Match Points:", "10", "goals", menu_width * 0.5f, 250, "arial75", 5, true));
	text_boxes[std::pair<int, int>(lobby_menu, lobby_points->box_element)] = lobby_points;
	text_boxes[std::pair<int, int>(lobby_menu, lobby_points->text_element)] = lobby_points;
	/*
	lobby_info = std::shared_ptr<PanelPlugin::Label>(new PanelPlugin::Label(lobby_menu, "Balls: 4 Match Points: 10", menu_width * 0.5f, 140, true, "arial75"));
	lobby_info->setColors(glm::vec4(.5, .0, .4, 0.4), glm::vec4(.5, 0, .4, 0.6), glm::vec4(0, 0, 0, 1), glm::vec4(0, 0, 0, 0));
	lobby_result = std::shared_ptr<PanelPlugin::Label>(new PanelPlugin::Label(lobby_menu, "Final Score: Blue 4, Red:9", menu_width * 0.5f, 250, true, "arial75"));
	lobby_result->setColors(glm::vec4(.5, .0, .4, 0.4), glm::vec4(.5, 0, .4, 0.6), glm::vec4(0, 0, 0, 1), glm::vec4(0, 0, 0, 0));
	*/
	lobby_teams = std::shared_ptr<PanelPlugin::MenuButton>(new PanelPlugin::MenuButton(lobby_menu, "Switch Teams", "Switch", menu_width / 2.0f, 1000, true, "arial100"));
	buttons[std::pair<int, int>(lobby_menu, lobby_teams->element)] = lobby_teams;

	lobby_back = std::shared_ptr<PanelPlugin::MenuButton>(new PanelPlugin::MenuButton(lobby_menu, "Leave" , "Leave", menu_width*0.25f, 1150, true, "arial100"));
	buttons[std::pair<int, int>(lobby_menu, lobby_back->element)] = lobby_back;

	lobby_countdown = std::shared_ptr<PanelPlugin::Label>(new PanelPlugin::Label(lobby_menu, "", menu_width * 0.75f, 1150, false, "arial100"));
	lobby_countdown->setColors(glm::vec4(.0, .0, .0, 0.0), glm::vec4(.0, 0, .0, 0.0), glm::vec4(.0, 0.0, 0.0, .0), glm::vec4(0, 0, 0, 0));

	game_start = std::shared_ptr<PanelPlugin::MenuButton>(new PanelPlugin::MenuButton(lobby_menu, "Start", "start_game", menu_width * 0.75f, 1150, true, "arial100"));
	buttons[std::pair<int, int>(lobby_menu, game_start->element)] = game_start;

	game_start_hidden = false; 


	start_nav_select[lobby_menu] = lobby_teams->element;

	
	glm::vec2 up = glm::vec2(0, 1);
	glm::vec2 down = glm::vec2(0, -1);
	glm::vec2 left = glm::vec2(-1, 0);
	glm::vec2 right = glm::vec2(1, 0);

	panels->createNavLink(lobby_menu, lobby_balls->box_element, down, lobby_menu, lobby_points->box_element) ;
	panels->createNavLink(lobby_menu, lobby_balls->box_element, up, lobby_menu, lobby_back->element);
	panels->createNavLink(lobby_menu, lobby_balls->box_element, left, lobby_menu, lobby_balls->box_element);
	panels->createNavLink(lobby_menu, lobby_balls->box_element, right, lobby_menu, lobby_balls->box_element);

	panels->createNavLink(lobby_menu, lobby_points->box_element, down, lobby_menu, lobby_teams->element);
	panels->createNavLink(lobby_menu, lobby_points->box_element, up, lobby_menu, lobby_balls->box_element);
	panels->createNavLink(lobby_menu, lobby_points->box_element, right, lobby_menu, lobby_points->box_element);
	panels->createNavLink(lobby_menu, lobby_points->box_element, left, lobby_menu, lobby_points->box_element);

	panels->clearNavfrom(lobby_menu, lobby_teams->element) ;
	panels->createNavLink(lobby_menu, lobby_teams->element, up, lobby_menu, lobby_points->box_element);
	panels->createNavLink(lobby_menu, lobby_teams->element, right, lobby_menu, lobby_teams->element);
	panels->createNavLink(lobby_menu, lobby_teams->element, left, lobby_menu, lobby_teams->element);
	panels->createNavLink(lobby_menu, lobby_teams->element, left + down, lobby_menu, lobby_back->element);
	panels->createNavLink(lobby_menu, lobby_teams->element, right + down, lobby_menu, game_start->element);
	
	panels->clearNavfrom(lobby_menu, lobby_back->element);
	panels->createNavLink(lobby_menu, lobby_back->element,  down, lobby_menu, lobby_balls->box_element);
	panels->createNavLink(lobby_menu, lobby_back->element, left, lobby_menu, lobby_back->element);
	panels->createNavLink(lobby_menu, lobby_back->element, up, lobby_menu, lobby_teams->element);
	panels->createNavLink(lobby_menu, lobby_back->element, right, lobby_menu, game_start->element);

	panels->createNavLink(lobby_menu, game_start->element, down, lobby_menu, lobby_balls->box_element);
	panels->createNavLink(lobby_menu, game_start->element, left, lobby_menu, lobby_back->element);
	panels->createNavLink(lobby_menu, game_start->element, up, lobby_menu, lobby_teams->element);
	panels->createNavLink(lobby_menu, game_start->element, right, lobby_menu, game_start->element);

}


void NarballMenu::updateLobbyMenu(std::shared_ptr<const Lobby> lobby){
	WorldPlugin* worlds = getTool<WorldPlugin>();
	
	PanelPlugin* panels = getTool<PanelPlugin>();

	double time_in_lobby = worlds->getWorldTime(NARBALL) - lobby->time_lobby_started;
	double countdown_time = lobby->min_time_in_lobby - time_in_lobby;

	if (countdown_time > 0) {
		lobby_countdown->setText(concat(" ", (int)countdown_time) + " ");
	}
	else {
		lobby_countdown->setText("");
	}

	if(hosting){
		lobby_balls->editable = true;
		lobby_points->editable = true;
		if(lobby_balls->getInt() != lobby->balls || lobby_points->getInt() != lobby->match_points){
			worlds->queue(NARBALL,lobby->id, &Lobby::setMatchParameters, lobby_balls->getInt(), lobby_points->getInt()) ;
		}
	
		if(game_start_hidden){
			PanelPlugin::DefaultInstance inst = panels->getPanelElementInstance<PanelPlugin::DefaultInstance>(lobby_menu, game_start->element);
			panels->clearKeyFrames(lobby_menu, game_start->element);
			inst.pose = buttons[std::pair<int, int>(lobby_menu, game_start->element)]->base_pose;
			panels->addPanelElementKeyFrame(lobby_menu, game_start->element, panels->getTime(), inst);
			game_start_hidden = false;

			
			glm::vec2 up = glm::vec2(0, 1);
			glm::vec2 down = glm::vec2(0, -1);
			glm::vec2 left = glm::vec2(-1, 0);
			glm::vec2 right = glm::vec2(1, 0);

			panels->clearNavfrom(lobby_menu, lobby_teams->element);
			panels->createNavLink(lobby_menu, lobby_teams->element, up, lobby_menu, lobby_points->box_element);
			panels->createNavLink(lobby_menu, lobby_teams->element, right, lobby_menu, lobby_teams->element);
			panels->createNavLink(lobby_menu, lobby_teams->element, left, lobby_menu, lobby_teams->element);
			panels->createNavLink(lobby_menu, lobby_teams->element, left + down, lobby_menu, lobby_back->element);
			panels->createNavLink(lobby_menu, lobby_teams->element, right + down, lobby_menu, game_start->element);

			panels->clearNavfrom(lobby_menu, lobby_back->element);
			panels->createNavLink(lobby_menu, lobby_back->element, down, lobby_menu, lobby_balls->box_element);
			panels->createNavLink(lobby_menu, lobby_back->element, left, lobby_menu, lobby_back->element);
			panels->createNavLink(lobby_menu, lobby_back->element, up, lobby_menu, lobby_teams->element);
			panels->createNavLink(lobby_menu, lobby_back->element, right, lobby_menu, game_start->element);

		}
	
	}else{
		lobby_balls->setText(concat("", lobby->balls));
		lobby_balls->editable = false;
		lobby_points->setText(concat("", lobby->match_points));
		lobby_points->editable = false;

		panels->clearKeyFrames(lobby_menu, game_start->element);
		PanelPlugin::DefaultInstance inst ;
		inst.pose = game_start->base_pose ;
		panels->addPanelElementKeyFrame(lobby_menu, game_start->element, panels->getTime(), inst);
	
		//if (!game_start_hidden) {
			inst = panels->getPanelElementInstance<PanelPlugin::DefaultInstance>(lobby_menu, game_start->element);
			panels->clearKeyFrames(lobby_menu, game_start->element);
			inst.pose = glm::mat4(0);
			panels->addPanelElementKeyFrame(lobby_menu, game_start->element, panels->getTime(), inst);
			game_start_hidden = true;
		//}

			glm::vec2 up = glm::vec2(0, 1);
			glm::vec2 down = glm::vec2(0, -1);
			glm::vec2 left = glm::vec2(-1, 0);
			glm::vec2 right = glm::vec2(1, 0);

			panels->clearNavfrom(lobby_menu, lobby_teams->element);
			panels->createNavLink(lobby_menu, lobby_teams->element, up, lobby_menu, lobby_points->box_element);
			panels->createNavLink(lobby_menu, lobby_teams->element, down, lobby_menu, lobby_back->element);


			panels->clearNavfrom(lobby_menu, lobby_back->element);
			panels->createNavLink(lobby_menu, lobby_back->element, down, lobby_menu, lobby_balls->box_element);
			panels->createNavLink(lobby_menu, lobby_back->element, up, lobby_menu, lobby_teams->element);

	}

	
	//lobby_result->setText(lobby->result);
	
	
	std::map<int,std::string> red_team ;
	std::map<int, std::string> blue_team;

	for(const auto& [ id, player] : lobby->players){
		if(player.team){
			red_team[id] = player.name ;
		}else{
			blue_team[id] = player.name;
		}
	}

	float start_y = 350 ;
	float y_step = 100 ;
	float red_x = menu_width * 0.25f ;
	float blue_x = menu_width * 0.75f;

	std::unordered_set<int> all_players ;
	float y = start_y ;
	for(auto& [ id ,name] : red_team){
		if(player_label.find(id) == player_label.end() || player_label[id]->text != name){
			player_label[id] = std::shared_ptr<PanelPlugin::Label>(new PanelPlugin::Label(lobby_menu, name, red_x, y, true, "arial60"));
			player_label[id]->setColors(glm::vec4(.5, .0, .4, 0.4), glm::vec4(.5, 0, .4, 0.6), glm::vec4(0, 0, 0, 1), glm::vec4(1, 0.5, 0.15, 1));
		}else{
			player_label[id]->moveTo(red_x, y, true, 0.25) ;
			//player_label[id]->setText(name) ;
		}
		all_players.insert(id);
		y += y_step ;
	}

	y = start_y;
	for (auto& [id, name] : blue_team) {
		if (player_label.find(id) == player_label.end() || player_label[id]->text != name) {
			player_label[id] = std::shared_ptr<PanelPlugin::Label>(new PanelPlugin::Label(lobby_menu, name, blue_x, y, true, "arial60"));
			player_label[id]->setColors(glm::vec4(.5, .0, .4, 0.4), glm::vec4(.5, 0, .4, 0.6), glm::vec4(0, 0, 0, 1), glm::vec4(1, 0.5, 0.15, 1));
		}else {
			player_label[id]->moveTo(blue_x, y, true, 0.25);
			//player_label[id]->setText(name);
		}
		all_players.insert(id);
		y += y_step;
	}

	for (auto& [ id, label] : player_label){
		if(all_players.find(id) == all_players.end()){ // player left
			player_label.erase(id);
		}
	}

}

float anim_time = 0.25 ;
void NarballMenu::hideLeft(int menu){
	PanelPlugin* panels = getTool<PanelPlugin>();
	if (!panels->hasPanel(menu)) {
		return;
	}
	double time = panels->getTime();
	panels->clearKeyFrames(menu);
	panels->addPanelKeyFrame(menu, time, menu_visible);
	panels->addPanelKeyFrame(menu, time + anim_time, left_menu_hide);
	panels->addPanelKeyFrame(menu, time + anim_time * 1.001f, menu_hidden);
	active_menu = -1 ;
}
void NarballMenu::showLeft(int menu){
	if (menu == active_menu) {
		return;
	}
	hideRight(active_menu);
	PanelPlugin* panels = getTool<PanelPlugin>();
	double time = panels->getTime();
	panels->clearKeyFrames(menu);
	panels->addPanelKeyFrame(menu, time , left_menu_hide);
	panels->addPanelKeyFrame(menu, time + anim_time , menu_visible);
	active_menu = menu ;
	if(last_active_pad >=0){
		panels->setNavSelection(active_menu,start_nav_select[active_menu]) ;
	}
}
void NarballMenu::hideRight(int menu) {
	PanelPlugin* panels = getTool<PanelPlugin>();
	if(!panels->hasPanel(menu)){
		return ;
	}
	double time = panels->getTime();
	panels->clearKeyFrames(menu) ;
	panels->addPanelKeyFrame(menu, time, menu_visible);
	panels->addPanelKeyFrame(menu, time + anim_time , right_menu_hide);
	panels->addPanelKeyFrame(menu, time + anim_time * 1.001f, menu_hidden);
	active_menu = -1 ;
}
void NarballMenu::showRight(int menu) {
	if(menu == active_menu){
		return ;
	}
	hideLeft(active_menu) ;
	PanelPlugin* panels = getTool<PanelPlugin>();
	double time = panels->getTime();
	panels->clearKeyFrames(menu);
	panels->addPanelKeyFrame(menu, time, right_menu_hide);
	panels->addPanelKeyFrame(menu, time + anim_time , menu_visible);
	active_menu = menu;
	if (last_active_pad >= 0) {
		panels->setNavSelection(active_menu, start_nav_select[active_menu]);
	}
}

void NarballMenu::enterPanel(int panel){
}

void NarballMenu::exitPanel(int panel){
}

void NarballMenu::enterPanelElement(int panel, int element) {
	PanelPlugin* panels = getTool<PanelPlugin>();
	AudioPlugin* sound = getTool<AudioPlugin>();
	VulkanPlugin* window = getTool<VulkanPlugin>();
	if (element >= 0 && buttons.find(std::pair<int, int>(panel, element)) != buttons.end()) {
		PanelPlugin::DefaultInstance inst = panels->getPanelElementInstance<PanelPlugin::DefaultInstance>(panel, element);
		panels->clearKeyFrames(panel, element);
		panels->addPanelElementKeyFrame(panel, element, panels->getTime(), inst);
		inst.pose = buttons[std::pair<int,int>(panel,element)]->hover_pose ;
		panels->addPanelElementKeyFrame(panel, element, panels->getTime() + 0.25, inst);
		//sound->play("tap", glm::vec3(0, 0, 0));
		sound->priorityPlay(TAP_SOUND, glm::vec3(0,0,0),0.5f,1,20) ;

		if (hovered_text_box != nullptr) {
			hovered_text_box->deselect();
			hovered_text_box = nullptr;
		}
		if (selected_text_box != nullptr) {
			selected_text_box->deselect();
			selected_text_box = nullptr;
		}

	}else if(text_boxes.find(std::pair<int, int>(panel, element)) != text_boxes.end()){ // entered a text box
		std::shared_ptr<PanelPlugin::TextBox>& hovered = text_boxes[std::pair<int, int>(panel, element)];
		if (!hovered->editable) {
			//a locked field, do nothing
		}
		else if (hovered_text_box != hovered.get()) {
			if (hovered_text_box != nullptr) {
				hovered_text_box->deselect();
			}
			if (selected_text_box != nullptr) {
				selected_text_box->deselect();
				selected_text_box = nullptr ;
			}
	
			hovered->select();
			hovered_text_box = hovered.get();

		}
		else {
		}
	}
	else if (panel >= 0) {// entered a panel but not an element

	}
}

void NarballMenu::exitPanelElement(int panel, int element) {
	PanelPlugin* panels = getTool<PanelPlugin>();

	if (element >= 0 && buttons.find(std::pair<int, int>(panel, element)) != buttons.end()) {
		PanelPlugin::DefaultInstance inst = panels->getPanelElementInstance<PanelPlugin::DefaultInstance>(panel, element);
		panels->clearKeyFrames(panel, element);
		panels->addPanelElementKeyFrame(panel, element, panels->getTime(), inst);
		inst.pose = buttons[std::pair<int, int>(panel, element)]->base_pose;
		panels->addPanelElementKeyFrame(panel, element, panels->getTime() + 0.25, inst);

	}
}


void NarballMenu::pressPanel(int panel, int element, int button){
	AudioPlugin* sound = getTool<AudioPlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>();
	VulkanPlugin* window = getTool<VulkanPlugin>();
	SteamworksPlugin* steam = getTool<SteamworksPlugin>();

	if(buttons.find(std::pair<int, int>(panel, element)) != buttons.end()){
		//sound->play("tap", glm::vec3(0, 0, 0));
		sound->priorityPlay(TAP_SOUND, glm::vec3(0, 0, 0), 1.0f, 1, 20);
		std::shared_ptr<PanelPlugin::MenuButton>& pressed = buttons[std::pair<int,int>(panel,element)] ;
		
		if(pressed->action == "Quick Play" && !trying_to_quickplay){
			steam->refreshServerList();
			trying_to_quickplay = true ;
		}


		if(pressed->action == "Private Host"){
			SteamworksPlugin::SteamServerInfo info;
			info.name = "Narball Server";
			info.map = "The Pool";
			info.max_players = 64;
			info.game_mode = "Classic";
			info.product_name = "Narball";
			info.product_description = "Be a narwhal. Hit a ball.";
			info.game_directory = "Narball" ;
			info.version = NARBALL_VERSION;
			std::shared_ptr<Socket> steam_socket = steam->hostPrivateLobby(info);

			if (worlds->host(steam_socket, NARBALL_VERSION)) {
				hosting = true;
				joined = false;

				worlds->createWorld(NARBALL, NARBALL_MAX_INFO_SPEED, NARBALL_MIN_EVENT_DURATION, NARBALL_MAX_READ_DISTANCE);
				worlds->setTimeSpeed(NARBALL, 1.0f); // worlds start paused

				auto lobby = std::make_shared<Lobby>(lobby_balls->getInt(), lobby_points->getInt());
				lobby_id = worlds->create(NARBALL, lobby);
				worlds->queue(NARBALL, lobby_id, &Lobby::addPlayer, local_player_id, player_name->getString());
				has_player = true;
				last_keep_alive = now();
				printf("Started hosting and added self to player list\n");

				showRight(lobby_menu);
			}
			else {
				printf("Hosting failed for some reason?\n");
			}
		}

		if (pressed->action == "Join") {
			//showRight(join_menu);
			SteamFriends()->ActivateGameOverlay("servers");
		}

		if (pressed->action == "Options") {
			showRight(options_menu);
		}

		if(pressed->action == "Back" ){
			if(active_menu == credits_menu){
				showLeft(options_menu) ;
			}else{
				showLeft(main_menu);
			}
		}

		if(pressed->action == "Leave"){
			if(joined){
				if(has_player){
					worlds->queue(NARBALL,lobby_id,&Lobby::removePlayer,local_player_id) ;
				}
				leaving = true ; // don't disconnect immediately to give the signal that we left a chance to get through
				showLeft(main_menu);
			}
			if(hosting){
				worlds->queue(NARBALL, lobby_id, &Lobby::setState, Lobby::CLOSED);
				leaving  = true ; // don't disconnect immediately to give the signal that we left a chance to get through
				showLeft(main_menu);
			}
			leave_time = now();
		}

		if(pressed->action == "Exit"){
			getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
		}
			
		if (pressed->action == "Switch") {
			worlds->queue(NARBALL,lobby_id,&Lobby::switchTeam, local_player_id);
		}

		if(pressed->action =="start_game" && worlds->amHosting()){
			worlds->queue(NARBALL, lobby_id, &Lobby::setState, Lobby::STARTED);
		}

		if(pressed->action == "music_up"){
			music_volume = std::min(100, music_volume+5) ; 
			music_volume_number->setText(concat("",music_volume)) ;
			sound->setGroupVolume(MUSIC_GROUP,music_volume*0.02f);
			
		}
		if (pressed->action == "music_down") {
			music_volume = std::max(0, music_volume - 5);
			music_volume_number->setText(concat("", music_volume));
			sound->setGroupVolume(MUSIC_GROUP, music_volume * 0.02f);

		}
		if (pressed->action == "effects_up") {
			effects_volume = std::min(100, effects_volume + 5);
			effects_volume_number->setText(concat("", effects_volume));
			sound->setGroupVolume(SOUND_EFFECTS_GROUP, effects_volume * 0.02f);

		}
		if (pressed->action == "effects_down") {
			effects_volume = std::max(0, effects_volume - 5);
			effects_volume_number->setText(concat("", effects_volume));
			sound->setGroupVolume(SOUND_EFFECTS_GROUP, effects_volume * 0.02f);
		}

		if (pressed->action == "toggle_debug") {
			printf("toggling debug\n");
			debug_panel_enabled = ! debug_panel_enabled ;
		}

		if (pressed->action == "toggle_fps") {
			if (target_fps == 0) {
				target_fps = 60;
				window->setVSync(false);
			}
			else {
				target_fps = 0;
				window->setVSync(true);
			}
		}

		if(pressed->action == "credits"){
			showRight(credits_menu) ;
		}

	}
		
	if(text_boxes.find(std::pair<int, int>(panel, element)) != text_boxes.end()) {
		std::shared_ptr<PanelPlugin::TextBox>& pressed = text_boxes[std::pair<int, int>(panel, element)];
		printf("panel %d, element %d, button %d\n", panel,element,button) ;
		if(button >= MOUSE_BUTTON_OFFSET){ // click a text box with a mouse button
			if( !pressed->editable){
				//clicked a locked field, do nothing
			}else if(selected_text_box != pressed.get() ){ 
				if(selected_text_box != nullptr){
					selected_text_box->deselect();
				}
				pressed->select();
				selected_text_box = pressed.get() ;

				//clear the field when clicking into certain fields that have a default value
			
				if(pressed == player_name && pressed->text == default_name){
					window->setTyped(0,"");
					pressed->update();
				}
			}else{
				pressed->select();
			}
		}else if(pressed.get() == lobby_balls.get() || pressed.get() == lobby_points.get()){ // also not a mouse click becaue its elseif
			int current_value = pressed->getInt();
			bool found = false;
			for(int k=0;k < textbox_button_values.size(); k++){ // select first number in value list bigger than current value
				if(textbox_button_values[k] > current_value){
					window->setTyped(0, concat("", textbox_button_values[k]));
					pressed->update();
					found = true ;
					break ;
				}
			}
			if(!found){ // if no numbers bigger then go back to start
				window->setTyped(0, concat("", textbox_button_values[0]));
				pressed->update();
			}
		}

	}else if(selected_text_box != nullptr){
		selected_text_box->deselect();
		selected_text_box = nullptr ;
	}

	

}

void NarballMenu::releasePanel(int panel, int element, int button){

}



void NarballMenu::onSteamGameExternalJoin(std::shared_ptr<SteamworksPlugin::SteamSocket> socket, const SteamworksPlugin::SteamServerInfo& server_info){
	printf("Got an external game join command from Steam!\n");
	WorldPlugin* worlds = getTool<WorldPlugin>();
	std::shared_ptr<Socket> steam_socket = socket ; // casting by creation avoids a warning
	worlds->connect(steam_socket, NARBALL_VERSION);
	join_start = now();
	lobby_id = -1;// we'll have to wait until we actually get the packet to find the lobby id and add ourselves
	waiting_on_join = true;
	join_start = now();
	
}



}// end namespace Narball