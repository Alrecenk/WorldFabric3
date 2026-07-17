#ifndef _NARBALL_GAME_H_
#define _NARBALL_GAME_H_ 1

#include "MachineState.h"
#include "WorldPlugin.h"
#include "ScenePlugin.h"
#include "ParticlePlugin.h"
#include "glm/glm.hpp"
#include "Utilities.h"

#include "NarballObjects.h"
#include "Ball.h"
#include "Narwhal.h"
#include "Nameplate.h"
#include "HighlightParticle.h"
#include <stdio.h>
#include <cstdlib>
#include <string>
#include <map>
#include <set>


namespace Narball{

class NarballGame : public MachineState {

public:

	static inline const std::string state_name = "narball_test";
	static inline bool wait_for_observables = true ; // whether the game waits for controls each frame before running World
	static inline bool wait_for_scene = true ; // whether the game waits for the scene to finish updating before rendering

	//Loads models from the hard drive on construction
	NarballGame();

	void run() override;

	// Called when switching into this sate before the first time run is claled
	void enter(std::shared_ptr<MachineState> from) override;


	// Called when switching outof this state after the last time run is called
	void exit(std::shared_ptr<MachineState> to) override;

	void updateScoreDisplay(std::shared_ptr<const Match> match) ;

private:

	bool controller_button_held = false ;
	bool space_held = false;
	bool leaving = false; 
	std::chrono::high_resolution_clock::time_point leave_time;

	std::vector<int> red_score_particles;
	std::vector<int> blue_score_particles;

	//Info about the world
	int64_t match_id = -1;
	std::shared_ptr<const Lobby> lobby;
	int pool_instance =-1 ;
	double start_time = 0 ;

	glm::vec3 camera_position = { 0,26,-12.0f }; // above and looking down and slightly forward
	glm::vec3 camera_look_at = { 0,0,0 };
	glm::vec3 score_position = {0,0,5} ;
	glm::vec3 countdown_position = { 0,0,0 };
	glm::vec3 end_result_position = { 0,0,3 };
	float fov = 0.35f;

	bool name_plates_visible = true ;

	std::shared_ptr<Nameplate> score_display ;
	std::map<int, std::shared_ptr<Nameplate>> score_nameplates;
	std::shared_ptr<Nameplate> new_display;
	std::string last_display_text = "" ; // last string displayed to score or countdown (so we can avoid regenerating the texture if it hasn't changed)

	glm::vec4 last_controls = glm::vec4(0);
	std::chrono::high_resolution_clock::time_point last_frame_time = now();
	std::chrono::high_resolution_clock::time_point last_control_time = now();
	std::chrono::high_resolution_clock::time_point last_keep_alive;
	std::chrono::high_resolution_clock::time_point last_create_request_time = now() ;
	
	double last_command_time = -1.0 ; // last control tiem in world time (prevents double send with manually timed events if UI ticks twice between world ticks)

};


} // end Narball namespace
#endif // #ifndef _NARBALL_GAME_H_