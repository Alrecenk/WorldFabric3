#ifndef _NARBALL_SERVER_H_
#define _NARBALL_SERVER_H_ 1

#include "MachineState.h"
#include "WorldPlugin.h"
#include "ScenePlugin.h"
#include "ParticlePlugin.h"
#include "glm/glm.hpp"
#include "Utilities.h"

#include "NarballObjects.h"
#include <stdio.h>
#include <cstdlib>
#include <string>
#include <map>
#include <set>


namespace Narball {

	class NarballServer : public MachineState {

	public:

		static inline const std::string state_name = "narball_server";
		static inline int min_players = 0 ; // Minimum number of players to start match
		static inline int match_points = 0; // Minimum number of players to start match
		static inline int num_balls = 0; // Minimum number of players to start match
		static inline float min_time_in_lobby = 5.0f ; // Mininum number of second ot spend in lobby screen befoe starting match
		
		NarballServer();

		void run() override;

		// Called when switching into this sate before the first time run is claled
		void enter(std::shared_ptr<MachineState> from) override;


		// Called when switching outof this state after the last time run is called
		void exit(std::shared_ptr<MachineState> to) override;

		// start a new game
		void startGame();


	private:
		int64_t lobby_id ;
		std::shared_ptr<const Lobby> lobby;
		int last_lobby_state = Lobby::CLOSED ;
		double time_lobby_started = 0;
		double last_kick_time = 0 ;
		float tick_interval = 1.0f / 60.0f;
		float ball_radius = 0.2f;
		bool match_closing = false;
		int64_t match_id ;
		static inline glm::vec2 min_arena = glm::vec2(-8, -4.5);
		static inline glm::vec2 max_arena = glm::vec2(8, 4.5);
		std::chrono::high_resolution_clock::time_point last_lobby_countdown_reset = now();
		
		
	};


} // end Narball namespace
#endif // #ifndef _NARBALL_SERVER_H_