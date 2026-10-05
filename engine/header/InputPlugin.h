#ifndef _INPUT_PLUGIN_H_
#define _INPUT_PLUGIN_H_ 1

#include "AsyncPlugin.h"
#include "Variant.h"
#include "Utilities.h"
#include <unordered_map>
#include <set>


class InputPlugin : public AsyncPlugin {

public:

	//Takes as input a path on disk to a json action file that matches the spec
	InputPlugin(const std::string input_action_file);

	~InputPlugin();

	// Called on every plug-in before any plug-ins are run
	void initialize() override;

	void run() override;


	enum InputType
	{
		BOOL,
		FLOAT,
		VEC2,
		VEC3,
		POSE
	};

	enum InputSource
	{
		STEAM_INPUT,
		OPEN_XR,
		SDL_KEY,
		SDL_MOUSE,
		SDL_GAMEPAD
	};

	struct Action{
		InputType type;
		std::string name;
		std::set<InputSource> bind;

		bool last_frame_bool = false;
		int last_frame = 0 ;
		bool last_bool  = false;

	};
		
	
	void parseConfig(Variant& config_file) ;

	int getActionID(const std::string& action) ;

	//Returns the current state ofa boolena input
	bool getBoolean(int action_id);

	bool getBoolean(const std::string& action);

	//If bool true this frame but not last frame
	bool getPressed(int action_id);

	bool getPressed(const std::string& action);

	//If bool false this frame but not last frame
	bool getReleased(int action_id);

	bool getReleased(const std::string& action);

	float getFloat(int action_id);

	glm::vec2 getVec2(int action_id);

	glm::vec3 getVec3(int action_id);

	glm::mat4 getPose(int acton_id);


	static inline std::map<std::string,InputType> type_string_to_enum ={
		{"boolean",BOOL},
		{"bool", BOOL},
		{"float",FLOAT},
		{"vec2", VEC2},
		{"vec3", VEC3},
		{"pose", POSE},
		{"mat4", POSE}
	};

	static inline std::map<std::string, InputSource>source_string_to_enum = {
		{"sdl_key", SDL_KEY},
		{"sdl_mouse", SDL_MOUSE},
		{"sdl_gamepad", SDL_GAMEPAD},
		{"sdl_controller", SDL_GAMEPAD},
		{"openxp", OPEN_XR},
		{"steam_input", STEAM_INPUT}
	} ;

	static inline std::map<std::string, SDL_KeyCode> sdl_key_to_code = {
		{"escape", SDLK_ESCAPE},
		{"esc", SDLK_ESCAPE},
		{"space", SDLK_SPACE},
		{"up", SDLK_UP}, //TODO add ALL the special keys in this SDLK enum for string conversion
		{"down", SDLK_DOWN},
		{"left", SDLK_LEFT},
		{"right", SDLK_RIGHT}
	};

	static inline std::map<std::string, int> sdl_mouse_to_index = {
		{"left", 1},
		{"right", 2},
		{"middle", 3}
	};


	SDL_KeyCode toKeycode(const std::string& in) {
		const std::string s = toLower(in);
		//First look for special keys i nthe map
		auto iter = sdl_key_to_code.find(s) ;
		if (iter != sdl_key_to_code.end()) {
			return iter->second;
		}
		if(s.length() != 1){
			printf("special character key binding not found: %s\n", in.c_str()) ;
			return SDLK_UNKNOWN;
		}

		char c = s.at(0);
		if (c >= 'a' && c <= 'z') {
			return (SDL_KeyCode)(SDLK_a + (c - 'a')); // Letters
		}else if (c >= '0' && c <= '9') {
			return (SDL_KeyCode)(SDLK_0 + (c - '0')); // Numbers
		}

		return SDLK_UNKNOWN;
	}

private:
	int frame = 0 ;
	Variant action_file;
	std::unordered_map<std::string, int> name_to_action_id ;
	std::unordered_map<int, Action> actions ;

	//The following maps contain the bindings within other systems
	//Key is always the action id, value isa vector of binding keys in that system
	std::unordered_map<int, std::vector<int64_t>> steam_input;
	std::unordered_map<int, std::vector<std::string>> open_xr;

	std::unordered_map<int, std::vector<SDL_KeyCode>> sdl_key;
	std::unordered_map<int, std::vector<int>> sdl_mouse;
	std::unordered_map<int, std::vector<std::pair<int,int>>> sdl_gamepad;

};
#endif // #ifndef _INPUT_PLUGIN_H_
