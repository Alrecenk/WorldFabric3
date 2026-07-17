#include "AudioPlugin.h"

#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"


AudioPlugin::AudioPlugin() {


}

AudioPlugin::~AudioPlugin() {
	printf("Deconstructing Audio Plugin...\n");
	alcMakeContextCurrent(NULL);
	alcDestroyContext(context);
	alcCloseDevice(output_device);
}

// Called on every plug-in before any plug-ins are run
// Adds an XRStatus object with the tag "xr_status" contain data other plugins can use to interact with the headset
void AudioPlugin::initialize() {

    /* initialize OpenAL context, asking for 44.1kHz to match HRIR data */
    ALCint contextAttr[] = { ALC_FREQUENCY,44100,0 };
    output_device = alcOpenDevice(NULL);
    context = alcCreateContext(output_device, contextAttr);
    alcMakeContextCurrent(context);
    
    moveListener(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1.0f), glm::vec3(0, 1.0f, 0));
 
    for (int k = 0; k < reserved_channels; k++) {
        reserved_source.push_back(createSource(glm::vec3(0, 0, 0)));
    }
    moveListener(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
    printf("Audio plugin initialized.\n");
}

void AudioPlugin::run() {
	// Clear completed buffers
	for (auto& [name, so] : source) {
		int buffers_processed;
		alGetSourcei(so.source, AL_BUFFERS_PROCESSED, &buffers_processed);
		while (buffers_processed-- > 0) {
			ALuint buffer;
			alSourceUnqueueBuffers(so.source, 1, &buffer);
		}
	}

	if(recording){
		captureMicrophoneData();
	}else if(input_device != nullptr){ // we were just recording but have stopped
		captureMicrophoneData();
		alcCaptureStop(input_device) ;
		alcCaptureCloseDevice(input_device); // clean up the input device
		input_device = nullptr ;
	}
}

// Move the listener
void AudioPlugin::moveListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) {
    this->position = position;
    this->forward = forward;
    this->up = up;
    alListener3f(AL_POSITION, position[0], position[1], position[2]);
    alListener3f(AL_VELOCITY, velocity[0], velocity[1], velocity[2]);
    float orient[6] = { forward[0], forward[1], forward[2], up[0], up[1], up[2]};
    alListenerfv(AL_ORIENTATION, orient);
}

// Move the listener to the position and orientation described by an openXR head matrix
void AudioPlugin::SetListenerToHMD(const glm::mat4 head_matrix) {
    glm::vec3 position = glm::vec3(head_matrix[3][0], head_matrix[3][1], head_matrix[3][2]);
    glm::vec3 forward = glm::vec3(-head_matrix[2][0], -head_matrix[2][1], -head_matrix[2][2]);
    glm::vec3 up = glm::vec3(head_matrix[1][0], head_matrix[1][1], head_matrix[1][2]);
    moveListener(position, forward, up);
}

// Create a new audio source
int AudioPlugin::createSource(const glm::vec3& position) {
	int source_id = next_source_id ;
	next_source_id++;
    Source &so = source[source_id];
    so.position = position;
    ALuint &s = so.source;
    alGenSources(1, &s);
    alSourcef(s, AL_PITCH, so.pitch);
    alSourcef(s, AL_GAIN, so.gain);
    alSource3f(s, AL_POSITION, so.position[0], so.position[1], so.position[2]);
    alSource3f(s, AL_VELOCITY, so.velocity[0], so.velocity[1], so.velocity[2]);
    alSourcei(s, AL_LOOPING, AL_FALSE);
	return source_id ;
}

//Move an existing audio source
void AudioPlugin::moveSource(int source_id, const glm::vec3& position) {
    Source& so = source[source_id];
    so.position = position;
    alSource3f(so.source, AL_POSITION, so.position[0], so.position[1], so.position[2]);
}

//Add a sound to the library
int AudioPlugin::addSound(SoundData& sound_data, int volume_group) {
	int sound_id = next_sound_id ;
	next_sound_id++;
    Sound& so = sound[sound_id];
    so.volume_group = volume_group;
    alGenBuffers(1, &(so.buffer));
    so.format = sound_data.format;
    so.sample_rate = sound_data.sample_rate;
    so.buffer_size =  sound_data.data.getSize()-4 ; // remove array length and get raw data byte size
    alBufferData(so.buffer, so.format, sound_data.data.getByteArray(), so.buffer_size, so.sample_rate);
	return sound_id ;
}

//Add a sound to the library from a WAV file
int AudioPlugin::addWAV(const std::string& file_path, int volume_group) {
    Variant file_data = Variant::loadFileBytes(file_path);
    if (file_data.defined()) {
        SoundData sound_data = AudioPlugin::loadWAV(file_data);
        return addSound( sound_data, volume_group);
    }else{
		return -1 ;
        printf("Could not load sound : %s\n", file_path.c_str());
    }
}

//Add a sound to the library from a OGG file
int AudioPlugin::addOGG(const std::string& file_path, int volume_group) {
	Variant file_data = Variant::loadFileBytes(file_path);
	if (file_data.defined()) {
		SoundData sound_data = AudioPlugin::loadOGG(file_data);
		return addSound(sound_data, volume_group);
	} else {
		return -1;
		printf("Could not load sound : %s\n", file_path.c_str());
	}
}

//queue a sound to play from the given source after it's current sounds have been played
void AudioPlugin::queueSound(int sound_id, int source_id) {
    if (sound.find(sound_id) == sound.end()) {
        return;
    }
    if (source[source_id].format == -1) {
        source[source_id].format = sound[sound_id].format;
    }
    if (source[source_id].format == sound[sound_id].format) {
        source[source_id].volume_group = sound[sound_id].volume_group;
        alSourceQueueBuffers(source[source_id].source, 1, &(sound[sound_id].buffer));
        alSourcePlay(source[source_id].source);
        source[source_id].last_queued = sound_id;
        alSourcef(source[source_id].source, AL_GAIN, source[source_id].gain * getGroupVolume(source[source_id].volume_group));
    }else{
        printf("Mixed format sounds (%d) on a single source (%d)is not allowed!\n", sound_id, source_id);
        printf(" %d   %d\n", sound[sound_id].format, source[source_id].format);
    }
}

// Returns the number of sounds currently queued to the given source
int AudioPlugin::amountQueued(int source_id) {
    if (source.find(source_id) == source.end()) {
        return 0;
    }
    int queued;
    alGetSourcei(source[source_id].source, AL_BUFFERS_QUEUED, &queued);
    return queued;
}

// Returns if amountQueued is greater than 0
bool AudioPlugin::active(int source_id) {
    return amountQueued(source_id) > 0;
}

// Delete a source
void AudioPlugin::deleteSource(int source_id) {
    alDeleteSources(1, &(source[source_id].source));
    source.erase(source_id);  
}

// Delete a sound
void AudioPlugin::deleteSound(int sound_id) {
    alDeleteBuffers(1, &(sound[sound_id].buffer));
    sound.erase(sound_id);
}

// return if a source with the given name is active
bool AudioPlugin::hasSource(int source_id) {
    return source.find(source_id) != source.end();
}

// return if a sound with the given name is active
bool AudioPlugin::hasSound(int sound_id) {
    return sound.find(sound_id) != sound.end();
}

//Sets the volume for a given source
void AudioPlugin::setSourceGain(int source_id, float gain) {
    source[source_id].gain = gain;
    alSourcef(source[source_id].source, AL_GAIN, gain * getGroupVolume(source[source_id].volume_group));
}

//Sets the pitch multiplier for a given source
void AudioPlugin::setSourcePitch(int source_id, float pitch) {
    source[source_id].pitch = pitch;
    alSourcef(source[source_id].source, AL_PITCH, pitch);
}

// Loads a .wav file into an object that can be added as a playable sound
AudioPlugin::SoundData AudioPlugin::loadWAV(Variant& file_bytes) {
    int magic_num = file_bytes.readInt(0);
    if (magic_num != 1179011410) { // integer for the ascii characters for RIFF indicating this is a wav file
        printf("Attempted to load WAV file that wasn't a wav file!\n");
        return AudioPlugin::SoundData();
    }

    int channels = (int)file_bytes.readShort(22);
    unsigned int sample_rate = (unsigned int)file_bytes.readInt(24);
    int bps = (int)file_bytes.readShort(34);
    int size = file_bytes.readInt(40);
    Variant data;
    data.makeFillableByteArray(size);
    file_bytes.readBytes(44, size, data.getByteArray());

    unsigned int format;
    if(channels == 1){
        if(bps == 8){
            format = AL_FORMAT_MONO8;
        }else{
            format = AL_FORMAT_MONO16;
        }
    }else{
        printf("Loaded stereo file which will not work with openAL 3D sound!\n");
        if (bps == 8){
            format = AL_FORMAT_STEREO8;
        }else{
            format = AL_FORMAT_STEREO16;
        }
    }
    return { format, sample_rate, data };
}

//Saves a SoundData object to a WAV file
void AudioPlugin::saveWAV(const SoundData& sound, const std::string& file_path) {
	int channels = 0;
	int bits_per_sample = 0;

	if (sound.format == AL_FORMAT_MONO16) {
		channels = 1; 
		bits_per_sample = 16;
	}else if (sound.format == AL_FORMAT_MONO8) {
		channels = 1; 
		bits_per_sample = 8;
	}else if (sound.format == AL_FORMAT_STEREO16) {
		channels = 2; 
		bits_per_sample = 16;
	}else if (sound.format == AL_FORMAT_STEREO8) {
		channels = 2; 
		bits_per_sample = 8;
	}else {
		printf("Unsupported format for WAV export!\n");
		return;
	}

	int data_size = sound.data.getSize() - 4; // remove array length and get raw data byte size
	int file_size = 44 + data_size;
	int byte_rate = sound.sample_rate * channels * (bits_per_sample / 8); 
	int block_align = channels * (bits_per_sample / 8);

	Variant file_bytes;
	file_bytes.makeFillableByteArray(file_size);
	unsigned char* header = file_bytes.getByteArray();

	*(int*)(header + 0) = 1179011410; // Magic number for "RIFF" header
	*(int*)(header + 4) = file_size - 8;

	header[8] = 'W'; 
	header[9] = 'A'; 
	header[10] = 'V';  // More header text
	header[11] = 'E';
	header[12] = 'f'; 
	header[13] = 'm'; 
	header[14] = 't'; 
	header[15] = ' ';

	*(int*)(header + 16) = 16; // Length of format data (16 for PCM)
	*(short*)(header + 20) = 1;// Audio format (1 for PCM)
	*(short*)(header + 22) = (short)channels;
	*(int*)(header + 24) = (int)sound.sample_rate;
	*(int*)(header + 28) = byte_rate;
	*(short*)(header + 32) = (short)block_align;
	*(short*)(header + 34) = (short)bits_per_sample;

	header[36] = 'd'; 
	header[37] = 'a'; 
	header[38] = 't'; 
	header[39] = 'a';
 
	*(int*)(header + 40) = data_size;
	std::memcpy(header + 44, sound.data.getByteArray(), data_size);
	file_bytes.saveBytesToFile(file_path);
}

// Loads a .ogg (ogg vorbis) file into an object that can be added as a playable sound
AudioPlugin::SoundData AudioPlugin::loadOGG(Variant& file_bytes){
	
	//Decode the file using stb_vorbis
	int channels = 0;
	int sample_rate = 0;
	short* pcm = nullptr;
	int samples_decoded = stb_vorbis_decode_memory(file_bytes.getByteArray(), file_bytes.getArrayLength(),
		&channels, &sample_rate, &pcm);
	if (samples_decoded <= 0) {
		printf("stb_vorbis: failed to decode OGG data (error code %d)\n", samples_decoded);
		return SoundData();
	}
	//Determine the open AL format
	ALenum al_format = 0;
	if (channels == 1){
		al_format = AL_FORMAT_MONO16;
	}else if (channels == 2){
		printf("Loaded stereo file which will not work with openAL 3D sound!\n");
		al_format = AL_FORMAT_STEREO16;
	}else {
		printf("Ogg file has %d channels – only mono or stereo are supported.\n", channels);
		free(pcm);
		return SoundData();
	}
	printf("Ogg file successfully decoded samples: %d\n", samples_decoded) ;
	//Put it as uncompressed raw data in the same format as WAV
	SoundData result;
	result.format = al_format;
	result.sample_rate = sample_rate;
	result.data = Variant((byte*)pcm, samples_decoded * channels * 2) ;

	free(pcm);

	return result ;
}


void AudioPlugin::play(int sound_id, glm::vec3 position, float gain) {
	play(sound_id,position, gain, 1.0f);
}

void AudioPlugin::play(int sound_id, glm::vec3 position) {
    play(sound_id, position, 1.0f, 1.0f);
}


// Plays a sound at the given location with adjusted pitch and gain
void AudioPlugin::play(int sound_id, glm::vec3 position, float gain, float pitch){
	int source_id = reserved_source[active_source];
	active_source = (active_source + 1) % reserved_source.size();
	if (amountQueued(source_id) > 0) { // if still playing a previous sound
		silence(source_id);
	}
	moveSource(source_id, position);
	setSourceGain(source_id, gain);
	setSourcePitch(source_id, pitch);
	queueSound(sound_id, source_id);

}

void AudioPlugin::priorityPlay(int sound_id, glm::vec3 position, float gain, int max_same, int max_sounds) {
	priorityPlay(sound_id, position, gain, 1.0f, max_same, max_sounds);
}


//Plays a sound only if there aren't too many of that sound or total sounds already playing closer
void AudioPlugin::priorityPlay(int sound_id, glm::vec3 position, float gain, float pitch, int max_same, int max_sounds) {
    float max2 = glm::dot(position - this->position, position - this->position);
    int total = 0;
    int same = 0;
    for (auto& [source_id, src] : source) {
        if (active(source_id)) {
            float d2 = glm::dot(src.position - this->position, src.position - this->position);
            if (d2 < max2) {
                total++;
                if (src.last_queued == sound_id) {
                    same++;
                }
            }
        }
    }
    if (total < max_sounds && same < max_same) {
        play(sound_id, position, gain, pitch);
    }

}

// Immediately stops and deques all sound on a source
void AudioPlugin::silence(int source_id) {
    if (source.find(source_id) == source.end()) {
        return;
    }
    Source& so = source[source_id];
    ALuint& s = so.source;
    alDeleteSources(1, &s); // Delete and recreate the openAL source with the same properties
    alGenSources(1, &s);
    alSourcef(s, AL_PITCH, so.pitch);
    alSourcef(s, AL_GAIN, so.gain);
    alSource3f(s, AL_POSITION, so.position[0], so.position[1], so.position[2]);
    alSource3f(s, AL_VELOCITY, so.velocity[0], so.velocity[1], so.velocity[2]);
    alSourcei(s, AL_LOOPING, AL_FALSE);
}

// Returns the total number of sounds currently playing
int AudioPlugin::countActive() {
    int count = 0;
    for (auto& [source_id, src] : source) {
        if (active(source_id)) {
            count++;
        }
    }
    return count;
}

// Returns the number of sound currently playing closer to the listener than the given position
int AudioPlugin::countActiveCloser(glm::vec3 position) {
    float max2 = glm::dot(position - this->position, position - this->position);
    int count = 0;
    for (auto& [source_id, src] : source) {
        if (active(source_id)) {
            float d2 = glm::dot(src.position - this->position, src.position - this->position);
            if (d2 < max2) {
                count++;
            }
        }
    }
    return count;
}


// Returns the number of instances of the given sound currently playing
int AudioPlugin::countActive(int sound_id) {
    int count = 0;
    for (auto& [source_id, src] : source) {
        if (active(source_id) && src.last_queued == sound_id) {
            count++;
        }
    }
    return count;
}

// Returns the number of instances of the given sound currently playing that are closer to the listener than position
int AudioPlugin::countActiveCloser(int sound_id, glm::vec3 position) {
    float max2 = glm::dot(position - this->position, position - this->position);
    int count = 0;
    for (auto& [source_id, src] : source) {
        if (active(source_id) && src.last_queued == sound_id) {
            float d2 = glm::dot(src.position - this->position, src.position - this->position);
            if (d2 < max2) {
                count++;
            }
        }
    }
    return count;
}


//Sets a multiplier for the volumes of everything in a volume group
void AudioPlugin::setGroupVolume(int volume_group, float volume) {
    group_volume[volume_group] = volume;
    for (auto& [source_id, source] : source) {
        if (source.volume_group == volume_group) {
            setSourceGain(source_id, source.gain);
        }
    }
}

//returns the current volume multiplier for a group
float AudioPlugin::getGroupVolume(int volume_group) {
    if (group_volume.find(volume_group) != group_volume.end()) {
        return group_volume[volume_group];
    }
    else if (volume_group > 0) {
        printf("Attempted to fetch volume for a group that doesn't exist: %d\n", volume_group);
    }
        return 0.0f;
}

//Creates a pure sine wave tone at the givne frequency for the given duration
AudioPlugin::SoundData AudioPlugin::createTone(float frequency, float duration, float fade) {
    int bps = 16;
    unsigned int format = AL_FORMAT_MONO16;
    unsigned int sample_rate = 48000;
    
    Variant data;
    int buffer = 16000; // need 1/3 of a second of 0s on the end to prevent clicking at the end because openAL reads in chunks that don't line up
    int size = (int)(duration * sample_rate);
    data.makeFillableShortArray(size + buffer);
    short* samples = data.getShortArray();
    for (int k = 0; k < size; k++) {
        float time = (k * duration / size);
        float value = sinf(time * frequency * 6.282318f);
        
        
        if (time < fade) {
            value *= time / fade;
        }
        if (time > duration - fade) {
            value *= 1.0f - (time - (duration - fade)) / fade;
        }
        
        samples[k] = (short)( value * 32700);
    }
    for (int k = size; k < size + buffer; k++) {
        samples[k] = 0;
    }

    return { format, sample_rate, data };
}


//Returns a playable sound by overlaying several notes
AudioPlugin::SoundData AudioPlugin::compose(std::vector<AudioPlugin::Note> notes) {
    float duration = 0;
    for (int k = 0; k < notes.size(); k++) {
        duration = fmax(duration, notes[k].start_time + notes[k].duration);
    }
    duration += 0.01f;
    int bps = 16;
    unsigned int format = AL_FORMAT_MONO16;
    unsigned int sample_rate = 48000;

    Variant data;
    int buffer = 16000; // need 1/3 of a second of 0s on the end to prevent clicking at the end because openAL reads in chunks that don't line up
    int size = (int)(duration * sample_rate) ;
    data.makeFillableShortArray(size + buffer);
    short* samples = data.getShortArray();


    for (int k = 0; k < size + buffer; k++) {
        samples[k] = 0;
    }

    for (auto& note : notes) {
        int start_k = (int)(note.start_time * size / duration);
        int end_k = (int)((note.start_time + note.duration) * size / duration);
        for (int k = start_k; k < end_k; k++) {
            float time = (k * duration / size);
            float b = (time - note.start_time) / (note.duration);
            float a = 1.0f - b;
            float volume = a * note.start_volume + b * note.end_volume;
            float frequency = a * note.start_frequency + b * note.end_frequency;
            float value = volume * sinf(time * frequency * 6.282318f);
            int new_sample = (int)(value * 32765) + (int)samples[k];
            new_sample = std::max(std::min(new_sample, 32765), -32765);
            samples[k] = (short)(new_sample);
        }
    }
       
    return { format, sample_rate, data };
}



float AudioPlugin::pianoFrequency(int key) {
    return (float)(440.0 * pow(1.05946309436, key - 49));
}

//Returns an approximation of a piano note where key in (1,88)
std::vector<AudioPlugin::Note> AudioPlugin::pianoNote(int key, float time, float duration, float volume) {
    std::vector<AudioPlugin::Note> notes;
    float fade = fmax(duration - 0.2f, duration*0.5f); //time where fade starts
    float base_f = pianoFrequency(key);
    //notes.emplace_back(time, fade, f, volume, f, volume);
    //notes.emplace_back(time+fade, duration-fade, f, volume, f, 0.0f);

    //std::vector<float> hf = {1, 2.0f, 3.01f, 3.97f, 5.05f, 6.1f };
    std::vector<float> hf = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
    std::vector<float> hv = { 1.0f, -0.3f, 0.2f, -0.1f, 0.05f, -0.02f };
    float hvt = 0;
    for (int k = 0; k < hv.size(); k++) {
        hvt += hv[k];
    }
    for (int k = 0; k < hv.size(); k++) {
        hv[k] /= hvt;
    }

    for (int h = 0; h < hf.size(); h++) {
        float f = base_f*hf[h];
        float v = hv[h]*volume;
        notes.emplace_back(time, fade, f , v, f , v);
        notes.emplace_back(time + fade, duration - fade, f, v, f , 0.0f);
    }

    return notes;

}

//Returns an approximation of a piano note where key in (1,88)
std::vector<AudioPlugin::Note> AudioPlugin::pianoChord(std::vector<int> key, float time, float duration, float volume) {
    std::vector<AudioPlugin::Note> notes;
    for (int k : key) {
        auto a = pianoNote(k, time, duration, volume / key.size());
        for (auto& n : a) {
            notes.push_back(n);
        }
    }
    return notes;
}


//Enable the default microhpone and begin saving audio into a buffer
void AudioPlugin::startRecording(){
	//Open the default microphone with a 1 second buffer
	const ALCchar* device_name = alcGetString(input_device, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);
	input_device = alcCaptureOpenDevice(device_name, recording_sample_rate, recording_format, recording_sample_rate);
	alcCaptureStart(input_device) ;
	printf("Recording on %s\n", device_name) ;
	lock.lock();
	microphone_stream.clear();
	microphone_stream.emplace_back().reserve(recording_sample_rate); // chunks of 1 second of mono data
	total_recorded_samples = 0;
	recording_samples_removed = 0 ;
	lock.unlock();
	recording = true ;
}

//Enable a microhpone whose name contains the given keyword and begin saving audio into a buffer
void AudioPlugin::startRecording(const std::string& keyword) {
	//Open the default microphone with a 1 second buffer

	
	const ALCchar* device_name = alcGetString(input_device, ALC_CAPTURE_DEVICE_SPECIFIER);
	//printf("%s\n", device_name) ;
	int offset = 0 ;
	const ALCchar* last_name = device_name ;
	//return of alcGetString is several null terminated strings in a row terminated with two null characters
	while((char)*(device_name + offset) != 0 || (char)*(device_name + offset + 1) != 0){
		if((char)*(device_name + offset) == 0){
			last_name = device_name + offset + 1 ;
			printf("%s\n", last_name) ;
			if(std::string(last_name).find(keyword) != std::string::npos){
				device_name = last_name ;
				break ;
			}
		}
		offset++;
	}
	//If We didn't find the keyword then it will be the first on the lis twhich also the default	
	input_device = alcCaptureOpenDevice(device_name, recording_sample_rate, recording_format, recording_sample_rate);
	alcCaptureStart(input_device);
	printf("Recording on %s\n", device_name);
	lock.lock();
	microphone_stream.clear();
	microphone_stream.emplace_back().reserve(recording_sample_rate); // chunks of 1 second of mono data
	total_recorded_samples = 0;
	recording_samples_removed = 0;
	lock.unlock();
	recording = true;

}

//Stop reading the active microphone
void AudioPlugin::stopRecording(){
	recording = false;
}

//Called on an independent thread to capture sound from OpenAL and place it into the recording buffer
void AudioPlugin::captureMicrophoneData() {
	int samples_available = 0 ;
	alcGetIntegerv(input_device, ALC_CAPTURE_SAMPLES, 1, &samples_available);
	if(samples_available > 0){
		alcCaptureSamples(input_device, recording_buffer.data(), samples_available);
		lock.lock();
		int samples_moved = 0 ;
		while(samples_moved < samples_available){
			std::vector<short>& latest_chunk = microphone_stream.back() ;
			int space_available = recording_sample_rate - (int)latest_chunk.size() ;
			if(space_available > samples_available - samples_moved){ // fits in the current latest chunk and doesn't fill it
				latest_chunk.insert(latest_chunk.end(), recording_buffer.begin() + samples_moved, recording_buffer.begin() + samples_available) ;
				samples_moved += samples_available ;
			}else{ // the latest chunk is filled
				latest_chunk.insert(latest_chunk.end(), recording_buffer.begin() + samples_moved, recording_buffer.begin() + samples_moved + space_available);
				microphone_stream.emplace_back().reserve(recording_sample_rate); // make a new chunk with reserved data for 1 second of mono sound
				samples_moved += space_available ;
			}
		}
		lock.unlock();
		total_recorded_samples += samples_available ;
	}
}

//Returns the total amount of samples recorded in the current microphone recording session
int AudioPlugin::getMicrophoneSessionSamples(){
	return total_recorded_samples ;
}

//Returns a playable sound object pulled from the microphone buffer
AudioPlugin::SoundData AudioPlugin::getMicrophoneSound(int export_start_sample, int export_samples){
	int chunk_start_sample = recording_samples_removed ; // first chunk's sample position relative to entire session streaming history
	int copied_so_far = 0 ;
	SoundData sound ;
	sound.format = recording_format ;
	sound.sample_rate = recording_sample_rate;
	sound.data.makeFillableShortArray(export_samples) ;
	short* exported_samples = sound.data.getShortArray() ;
	lock.lock();
	for(std::vector<short>& chunk : microphone_stream){
		if(chunk_start_sample < export_start_sample + export_samples && export_start_sample < chunk_start_sample + chunk.size()){// chunk overlaps range we want
			int copy_start_index = std::max(0, export_start_sample - chunk_start_sample) ;
			int amount_to_copy = std::min(export_samples-copied_so_far, (int)chunk.size() - copy_start_index) ;
			memcpy(exported_samples + copied_so_far,chunk.data() + copy_start_index,amount_to_copy * sizeof(short)) ;
			copied_so_far += amount_to_copy ;
		}
		chunk_start_sample += (int)chunk.size();
	}
	lock.unlock();
	return sound ;
}

// Delete old microphone recordintg data, guranteed to keep at least minimum_samples_to_keep of the most recent data
void AudioPlugin::trimRecordingBuffer(int minimum_samples_to_keep){
	int chunks_to_keep = minimum_samples_to_keep / recording_sample_rate + 2 ;
	lock.lock();
	while(microphone_stream.size() > chunks_to_keep){
		recording_samples_removed += (int)microphone_stream.front().size() ;
		microphone_stream.pop_front() ;
	}
	lock.unlock();
}



AudioPlugin::SoundData::SoundData(std::vector<float> signal, int sample_rate){
	format = AL_FORMAT_MONO16 ;
	this->sample_rate = sample_rate ;
	data.makeFillableShortArray((int)signal.size()) ;
	short* shorts = data.getShortArray();
	for (int k = 0; k < signal.size(); k++) {
		shorts[k]  = (short)(fmin(1.0f,fmax(-1.0f,signal[k])) * 32767.0f) ;
	}
}

//Returns a signal of mono sound data, mapped to -1 to 1
std::vector<float> AudioPlugin::SoundData::getNormalizedSignal(){
	if(format == AL_FORMAT_MONO16){
		int samples = data.getArrayLength();
		if(data.type_ == Variant::BYTE_ARRAY){ // Variant type might not match format type if it came from a file
			samples/=2 ; // format type is right, so adjust sample count
		}
		short* shorts = data.getShortArray();

		std::vector<float> signal(samples,0) ;

		float scale = 1.0f / 32768.0f; 
		for(int k=0;k<samples;k++){
			signal[k] = shorts[k] * scale;
		}
		return signal ;
	}else{

		printf("normalized signal currently only avilable for AL_FORMAT_MONO16 :( \n");
		return std::vector<float>() ;
	}

}
