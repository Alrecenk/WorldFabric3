#ifndef _PLAY_SONG_H_
#define _PLAY_SONG_H_ 1

#include "MachineState.h"
#include "Variant.h"
#include "AudioPlugin.h"
#include "glm/glm.hpp"
#include "SavePlugin.h"

#include <stdio.h>
#include <cstdlib>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>




class PlaySong : public MachineState {

public:	
	static inline const std::string state_name = "song_state";


	//Loads models from the hard drive on construction
	PlaySong();


	void run() override;

	// Called when switching into this sate before the first time run is claled
	void enter(std::shared_ptr<MachineState> from) override;


	// Called when switching outof this state after the last time run is called
	void exit(std::shared_ptr<MachineState> to) override;


	

private:

	AudioPlugin* audio = nullptr;
	long last_play_time = 0;
	glm::vec3 z = glm::vec3 (0.0f, 0.0f, 0.0f);
	std::vector<int> notes = { 40,39,40,39,40,35,38,36,33,0,24,28,33,35,0,28,32,35,36,0,28};
	std::map<int,int> piano_key_sound ;
	int next_note = 0;

};
#endif // #ifndef _PLAY_SONG_H_
