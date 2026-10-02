#ifndef _INPUT_PLUGIN_H_
#define _INPUT_PLUGIN_H_ 1

#include "AsyncPlugin.h"
#include "Variant.h"
#include "Utilities.h"
#include <unordered_map>


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
		std::vector<InputSource> bind;

		bool last_frame_bool = false;
		int last_frame = 0 ;
		bool last_bool  = false;

	};
		
	
	void parseConfig(Variant& config_file) ;

	int getActionID(const std::string& action) ;

	bool getBoolean(int action_id);

	//If bool true this frame but not last frame
	bool getPressed(int action_id);

	//If bool false this frame but not last frame
	bool getReleased(int action_id);

	float getFloat(int action_id);

	glm::vec2 getVec2(int action_id);

	glm::vec3 getVec3(int action_id);

	glm::mat4 getPose(int acton_id);


private:
	int frame = 0 ;
	Variant action_file;
	std::unordered_map<std::string, int> name_to_action_id ;
	std::unordered_map<int, Action> actions ;

	//The following maps contain the bindings within other systems
	//Key is always the action id, value isa vector of binding keys in that system
	std::unordered_map<int, std::vector<int64_t>> steam_input;
	std::unordered_map<int, std::vector<std::string>> open_xr;

	std::unordered_map<int, std::vector<int>> sdl_key;
	std::unordered_map<int, std::vector<int>> sdl_mouse;
	std::unordered_map<int, std::vector<std::pair<int,int>>> sdl_gamepad;

	Variant action_file ;
};
#endif // #ifndef _INPUT_PLUGIN_H_
