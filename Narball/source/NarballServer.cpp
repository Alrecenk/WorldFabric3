#include "NarballServer.h"

#include "ScenePlugin.h"
#include "WorldPlugin.h"
#include "ParticlePlugin.h"
#include "AudioPlugin.h"
#include "Utilities.h"
#include "FlagSet.h"
#include "NarballMenu.h"
#include "StatePlugin.h"


#include <queue>

namespace Narball {
	
	//Loads models from the hard drive on construction
	NarballServer::NarballServer() {
		
	}


	// Called when switching into this state before the first time run is claled
	void NarballServer::enter(std::shared_ptr<MachineState> from) {
		WorldPlugin* worlds = getTool<WorldPlugin>();
		SteamworksPlugin* steam = getTool<SteamworksPlugin>();
		
		std::shared_ptr<Socket> steam_socket = steam->getActiveSocket() ;// Dedicated server creates a socket on boot

		if (worlds->host(steam_socket, NARBALL_VERSION)) {

			worlds->createWorld(NARBALL, NARBALL_MAX_INFO_SPEED, NARBALL_MIN_EVENT_DURATION, NARBALL_MAX_READ_DISTANCE);
			worlds->setTimeSpeed(NARBALL, 1.0f); // worlds start paused

			auto lobby = std::make_shared<Lobby>(num_balls, match_points);
			lobby_id = worlds->create(NARBALL, lobby);


			//printf("Lobby Opened!\n");
		}else{
			printf("Something went wrong hosting the world!?\n");
		}
		
	}

	void NarballServer::run() {
		// get all the tools we'll be using
		WorldPlugin* worlds = getTool<WorldPlugin>();
		SteamworksPlugin* steam = getTool<SteamworksPlugin>();

		lobby = worlds->observe<Lobby>(NARBALL, lobby_id);
		double current_time = worlds->getWorldTime(NARBALL) ;

		//Detect when the game has moved back to the lobby
		if(lobby && lobby->state != last_lobby_state){
			if(lobby->state == Lobby::OPEN){
				time_lobby_started = current_time;
				worlds->queue(NARBALL, lobby->id, &Lobby::setTimeLobbyStarted, time_lobby_started);
				worlds->queue(NARBALL, lobby->id, &Lobby::setMinTimeInLobby, min_time_in_lobby);
			}
			last_lobby_state = lobby->state ;
		}

		// Reset the wait time in lobby while there aren't enough playhers
		if(lobby && lobby->state == Lobby::OPEN && lobby->players.size() < min_players){
			time_lobby_started = current_time;


			if (millisBetween(last_lobby_countdown_reset, now()) > 500) {
				worlds->queue(NARBALL, lobby->id, &Lobby::setTimeLobbyStarted, current_time);
				last_lobby_countdown_reset = now();
			}
		}

		//After a set time at the lobby (with enough players), start a new match
		if(lobby && last_lobby_state == Lobby::OPEN &&  current_time - time_lobby_started > min_time_in_lobby ){
			startGame();
		}

		//periodically kick players who haven't checked in
		if(lobby && current_time - last_kick_time > kick_interval){
			worlds->queue(NARBALL,lobby_id,&Lobby::kickDisconnected) ;
			last_kick_time = current_time ;
		}

		//Stop a running match if all players leave and go back to the lobby
		if(!match_closing && lobby && lobby->state == Lobby::STARTED && lobby->players.size() == 0){
			printf("Closing match because everyone left.\n");
			worlds->queue(NARBALL, match_id, &Match::closeMatch);
			match_closing = true; // guards against calling xosematch multiple times while it is running
		}
		if(lobby && match_closing && lobby->state == Lobby::OPEN){
			match_closing = false ;
		}
	

	}

	void NarballServer::startGame(){
		WorldPlugin* worlds = getTool<WorldPlugin>();
		lobby = worlds->observe<Lobby>(NARBALL, lobby_id);
		if (!lobby) {
			printf("could not find lobby!?\n");
		}

		printf("Match Starting.\n");

		worlds->queue(NARBALL, lobby_id, &Lobby::setState, Lobby::STARTED);
		time_lobby_started = FLT_MAX ; // amkes sure we don't start a game twice before the lobby switches state

		//Adjust the meta paramters based on the number of balls
		int grid_width = 32;
		int grid_height = 18;

		if (lobby->balls <= 20) {
			tick_interval = 1.0f / 240.0f;
			ball_radius = 0.20f;
			grid_width = 16;
			grid_height = 9;
		}else if (lobby->balls <= 100) {
			tick_interval = 1.0f / 120.0f;
			ball_radius = 0.20f;
			grid_width = 16;
			grid_height = 9;
		}else if (lobby->balls <= 300) {
			ball_radius = 0.20f;
			tick_interval = 1.0f / 60.0f;
		}else if (lobby->balls <= 600) {
			ball_radius = 0.15f;
			tick_interval = 1.0f / 60.0f;
		}else if (lobby->balls <= 1000) {
			ball_radius = 0.11f;
			tick_interval = 1.0f / 60.0f;
		}else {
			ball_radius = 0.1f;
			tick_interval = 1.0f / 30.0f;
		}

		double current_time = worlds->getWorldTime(NARBALL);
		double start_time = current_time + 3;
		auto grid = std::make_shared<Match>(lobby->id, glm::vec3(min_arena.x, -1, min_arena.y), glm::vec3(max_arena.x, 0, max_arena.y), grid_width, grid_height, tick_interval, start_time, ball_radius);
		match_id = worlds->create(NARBALL, grid);
		worlds->queue(NARBALL, match_id, &Match::createMatch);
		double ball_spawn_start = 0.2; // wait a bit to start spawning balls, so grid can initialize
		for (int k = 0; k < lobby->balls; k++) {
			auto ball = std::make_shared<Ball>(glm::vec3(-7 + randomFloat() * 14, 0, -4 + randomFloat() * 8), match_id);
			int64_t ball_id = worlds->create(NARBALL, ball, current_time + ball_spawn_start + 0.0005 * k); // create a bit in the future to give the grid a chance to initialize
			worlds->queue(NARBALL, ball_id, current_time + ball_spawn_start + 0.1 + 0.0005 * k, &Ball::update); // 0.1 seconds later start the ball moving
		}

		

	}

	// Called when switching out of this state after the last time run is called
	void NarballServer::exit(std::shared_ptr<MachineState> to) {
		//This shouldn't be called except maybe when exiting
	}


} // end namespace Narball