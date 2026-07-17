#ifndef _AUDIO_PLUGIN_H_
#define _AUDIO_PLUGIN_H_ 1

#include "AsyncPlugin.h"
#include "Variant.h"
#include "Utilities.h"

#include "openAL/al.h"
#include "openAL/alc.h"

#include <stdio.h>
#include <cstdlib>

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <deque>
#include <vector>

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"


class AudioPlugin : public AsyncPlugin {

public:

	static inline int reserved_channels = 32; // how many sources are reserved for simple play calls
	std::vector <int> reserved_source; // rotating sound sources allows playing overlapping sounds on demand
	int active_source = 0; // next reserved source to be used for simple sounds

	static const inline int MUSIC_GROUP = 10;
	static const inline int EFFECT_GROUP = 11;
	static const inline int VOICE_GROUP = 12;

	class SoundData {
	public:
		unsigned int format=0;
		unsigned int sample_rate=0;
		Variant data;


		SoundData() = default;

		SoundData(unsigned int f, unsigned int sr, Variant d)
			: format(f), sample_rate(sr), data(std::move(d)) {
		}

		//Create a 16 bit mono stream from a normalized signal
		SoundData(std::vector<float> signal, int sample_rate) ;

		//Returns a signal of mono sound data, mapped to -1 to 1
		std::vector<float> getNormalizedSignal() ;
	};

	class Source {
	public:
		ALuint source;
		int format = -1; // set when the first thing is queued and then everting else must be the same
		float gain = 1.0f;
		float pitch = 1.0f;
		glm::vec3 position = glm::vec3(0.0f,0.0f,0.0f);
		glm::vec3 velocity = glm::vec3(0.0f, 0.0f, 0.0f);
		int last_queued = -1;
		int volume_group = 0; // source volume group is set to a aoun when it is played or queued
	};

	class Sound {
	public:
		ALuint buffer;
		int buffer_size;
		int format;
		int sample_rate;
		int volume_group = 0;
	};

	class Note {
	public:
		float start_time = 0;
		float duration = 1;
		float start_frequency = 440.0f;
		float start_volume = 0.5f;
		float end_frequency = 440.0f;
		float end_volume = 0.5f;
	};

	AudioPlugin();

	~AudioPlugin();

	// Called on every plug-in before any plug-ins are run
	void initialize() override;

	void run() override;

	// Move the listener
	void moveListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up);

	// Move the listener to the position and orientation described by an openXR head matrix
	void SetListenerToHMD(const glm::mat4 head_matrix);

	// Create a new audio source
	int createSource(const glm::vec3& position);

	//Move an existing audio source
	void moveSource(int source_id, const glm::vec3& position);

	//Add a sound to the library
	int addSound(SoundData& sound_data, int volume_group);

	//Add a sound to the library from a WAV file
	int addWAV(const std::string& file_path, int volume_group);

	//Add a sound to the library from an OGG file
	int addOGG(const std::string& file_path, int volume_group);

	//queue a sound to play from the given source after it's current sounds have been played
	void queueSound(int sound_id, int source_id);

	// Immediately stops and deques all sound on a source
	void silence(int source_id);

	// Returns the number of sounds currently queued to the given source
	int amountQueued(int source_id);

	// Returns if amountQueued is greater than 0
	bool active(int source_id);

	// Delete a source
	void deleteSource(int source_id);

	// Delete a sound
	void deleteSound(int sound_id);

	// return if a source with the given name is active
	bool hasSource(int source_id);

	// return if a soundwith the given name is active
	bool hasSound(int sound_id);

	//Sets the volume for a given source
	void setSourceGain(int source_id, float gain);

	//Sets the pitch multiplier for a given source
	void setSourcePitch(int source_id, float pitch);
	
	// Loads a .wav file into an object that can be added as a playable sound
	static SoundData loadWAV(Variant& file_bytes);

	//Saves a SoundData object to a WAV file
	static void saveWAV(const SoundData& sound, const std::string& file_path) ;

	// Loads a .ogg (ogg vorbis) file into an object that can be added as a playable sound
	static SoundData loadOGG(Variant& file_bytes);

	// Plays a sound at the given location with a gain of 1
	void play(int sound_id, glm::vec3 position);

	// Plays a sound at the given location
	void play(int sound_id, glm::vec3 position, float gain);

	// Plays a sound at the given location with adjusted pitch and gain
	void play(int sound_id, glm::vec3 position, float gain, float pitch);

	// Returns the total number of sounds currently playing
	int countActive();

	// Returns the number of sound currently playing closer to the listener than the given position
	int countActiveCloser(glm::vec3 position);

	// Returns the number of instances of the given sound currently playing
	int countActive(int sound_id);

	// Returns the number of instances of the given sound currently playing that are closer to the listener than position
	int countActiveCloser(int sound_id, glm::vec3 position);

	//Plays a sound only if there aren't too many of that sound or total sounds already playing closer
	void priorityPlay(int sound_id, glm::vec3 position, float gain, int max_same, int max_sounds);

	//Plays a sound only if there aren't too many of that sound or total sounds already playing closer
	void priorityPlay(int sound_id, glm::vec3 position, float gain, float pitch, int max_same, int max_sounds);

	//TODO some way to externally control the distancemodel in openAL

	//Creates a pure sine wave tone at the givne frequency for the given duration
	SoundData static createTone(float frequency, float duration, float fade);

	//Returns a playable sound by overlaying several notes
	SoundData static compose(std::vector<Note> notes);

	// returns the frequency of a piano key
	static float pianoFrequency(int key);

	//Returns an approximation of a piano note where key in (1,88)
	std::vector<Note> static pianoNote(int key, float time, float duration, float volume);

	//Returns an approximation of a piano note where key in (1,88)
	std::vector<Note> static pianoChord(std::vector<int> key, float time, float duration, float volume);

	//Sets a multiplier for the volumes of everything in a volume group
	void setGroupVolume(int volume_group, float volume);

	//returns the current volume multiplier for a group
	float getGroupVolume(int volume_group);

	//Enable the default microhpone and begin saving audio into a buffer
	void startRecording() ;

	//Enable a microhpone whose name contains the given keyword and begin saving audio into a buffer
	void startRecording(const std::string& keyword);

	//Stop reading the active microphone
	void stopRecording();

	//Returns the total amount of samples recorded in the current microphone recording session
	int getMicrophoneSessionSamples();

	//Returns a playable sound object pulled from the microphone buffer
	SoundData getMicrophoneSound(int start_sample, int num_samples) ;

	// Delete old microphone recordintg data, guranteed to keep at least minimum_samples_to_keep of the most recent data
	void trimRecordingBuffer(int minimum_samples_to_keep) ;


	

private:

	ALCdevice* output_device = nullptr;
	ALCcontext* context = nullptr;

	// active sources and sounds
	std::map<int, Source> source;
	std::map<int, Sound> sound;
	std::map<int, float> group_volume;
	
	//Listener position data
	glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 forward = glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 velocity = glm::vec3(0.0f, 0.0f, 0.0f);
	int next_source_id = 1;
	int next_sound_id = 1 ;

	//Microphone data
	bool recording = false;
	ALCdevice* input_device = nullptr;
	std::deque<std::vector<short>> microphone_stream ; // Each chunk is 1 second of 16 bit mono data
	int recording_samples_removed = 0 ; //amount of samples i nthe current recording session that have been trimmed and are no longer available
	int total_recorded_samples = 0 ;
	int recording_sample_rate  = 48000;
	std::vector<short> recording_buffer = std::vector<short>(recording_sample_rate) ;
	ALCenum recording_format = AL_FORMAT_MONO16 ;

	//Called on an independent thread to capture sound from penAL and place it into the recordintg buffer
	void captureMicrophoneData() ;

};
#endif // #ifndef _AUDIO_PLUGIN_H_
