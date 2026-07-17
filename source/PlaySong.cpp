#include "PlaySong.h"

//Loads models from the hard drive on construction
PlaySong::PlaySong() {

}


void PlaySong::run() {
	
	long time = timeMilliseconds();
	if (time > last_play_time + 200) {
		if (notes[next_note]) {
			audio->play(piano_key_sound[ notes[next_note]], z);
		}
		next_note = (next_note + 1) % notes.size();
		last_play_time = time;
		
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

// Called when switching into this sate before the first time run is claled
void PlaySong::enter(std::shared_ptr<MachineState> from) {
	audio = getTool<AudioPlugin>();

	
	for (int k = 1; k < 88; k++) {
		AudioPlugin::SoundData tone = audio->compose(audio->pianoNote(k, 0, 0.25f, 0.5f));
		piano_key_sound[k] = audio->addSound(tone, AudioPlugin::MUSIC_GROUP);
	}
	
	
	/*
	if (!audio->hasSound("c")) {
		AudioPlugin::SoundData c_tone = audio->createTone(261.63f, 1.0f, 1.0f);
		audio->addSound("c", c_tone);
	}
	*/
	last_play_time = timeMilliseconds();
}


// Called when switching outof this state after the last time run is called
void PlaySong::exit(std::shared_ptr<MachineState> to) {

}
