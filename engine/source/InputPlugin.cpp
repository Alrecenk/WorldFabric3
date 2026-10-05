#include "InputPlugin.h"
#include "Utilities.h"
#include "VulkanPlugin.h"
#include "OpenXRPlugin.h"
#include "SteamworksPlugin.h"

//Takes as input a path on disk to a json action file that matches the spec
InputPlugin::InputPlugin(const std::string input_action_file){
	action_file = Variant::loadJSONFile(input_action_file) ;
	//action_file.printFormatted();
	parseConfig(action_file);
}

InputPlugin::~InputPlugin(){

}

// Called on every plug-in before any plug-ins are run
void InputPlugin::initialize(){

}

void InputPlugin::run(){
	frame++;
}


void InputPlugin::parseConfig(Variant& config_file){
	int num_actions = action_file["actions"].getArrayLength();
	for(int a=0;a<num_actions;a++){
		Action act;
		act.name = action_file["actions"][a]["name"].getString() ;
		act.type = type_string_to_enum[toLower(action_file["actions"][a]["type"].getString())];
		int num_bindings = action_file["actions"][a]["binding"].getArrayLength();
		for(int b = 0 ;b < num_bindings;b++){
			Variant bind = action_file["actions"][a]["binding"][b] ;
			InputSource source = source_string_to_enum[toLower(bind["source"].getString())] ;
			act.bind.insert(source);

			if(source == SDL_KEY){
				if(bind["path"].type_ == Variant::INT){
					int key_code = bind["path"].getInt();
					sdl_key[a].push_back((SDL_KeyCode)key_code) ;
					//printf("bound key code :%d\n", key_code);
				}else if(bind["path"].type_ == Variant::STRING){
					sdl_key[a].push_back(toKeycode(bind["path"].getString())) ;
					//printf("bound key code :%d\n", sdl_key[a][sdl_key[a].size()-1]);
				}
			}else if(source == SDL_MOUSE){
				if (bind["path"].type_ == Variant::INT) {
					sdl_mouse[a].push_back(bind["path"].getInt());
					//printf("bound mouse key:%d\n", bind["path"].getInt()) ;
				}else{
					sdl_mouse[a].push_back(sdl_mouse_to_index[toLower(bind["path"].getString())]) ;
				}
			}else if(source == SDL_GAMEPAD){
				//TODO
			}else if (source == STEAM_INPUT) {
				//TODO
			}else if (source == OPEN_XR) {
				//TODO
			}
		}
		actions[a] = act ;
		name_to_action_id[act.name] = a ;
	}

}

int InputPlugin::getActionID(const std::string& action){
	auto iter = name_to_action_id.find(action);
	if(iter != name_to_action_id.end()){
		return iter->second ;
	}
	return -1 ;
}

bool InputPlugin::getBoolean(int action_id){
	auto iter = actions.find(action_id) ;
	if (iter == actions.end()) {
		return false;
	}
	
	Action& action = iter->second;
	if(action.type != BOOL){
		return false;
	}
	bool value = false;
	for(const InputSource& bind : action.bind){
		if(bind == STEAM_INPUT){
			//TODO
		}else if(bind == OPEN_XR){
			//TODO
		}else if(bind == SDL_KEY){
			for(auto& code : sdl_key[action_id]){
				value |= getTool<VulkanPlugin>()->keyDown(code);
			}
		}else if (bind == SDL_MOUSE) {
			for (auto& index : sdl_mouse[action_id]) {
				value |= getTool<VulkanPlugin>()->mouseDown(index);
			}
		}else if (bind == SDL_GAMEPAD) {
			//TODO
		}
	}

	if(frame != action.last_frame){ // on a new frame
		action.last_frame_bool = action.last_bool ; // last frame bool becomes last bool we got in that frame
		action.last_frame = frame ;
	}
	action.last_bool = value ;
	return value ;

}

bool InputPlugin::getBoolean(const std::string& action){
	return getBoolean(getActionID(action)) ;
}

//If bool true this frame but not last frame
bool InputPlugin::getPressed(int action_id){
	bool value = getBoolean(action_id) ;
	auto iter = actions.find(action_id);
	if (iter == actions.end()) {
		return false;
	}
	Action& action = iter->second;
	if (action.type != BOOL) {
		return false;
	}
	return  value && ! action.last_frame_bool ;
}

bool InputPlugin::getPressed(const std::string& action) {
	return getPressed(getActionID(action));
}

//If bool false this frame but not last frame
bool InputPlugin::getReleased(int action_id){
	bool value = getBoolean(action_id);
	auto iter = actions.find(action_id);
	if (iter == actions.end()) {
		return false;
	}
	Action& action = iter->second;
	if (action.type != BOOL) {
		return false;
	}
	return !value && action.last_frame_bool;
}

bool InputPlugin::getReleased(const std::string& action) {
	return getReleased(getActionID(action));
}

float InputPlugin::getFloat(int action_id){
	auto iter = actions.find(action_id);
	if (iter == actions.end()) {
		return 0;
	}

	Action& action = iter->second;
	if (action.type != FLOAT) {
		return 0;
	}
	float value = 0;
	for (const InputSource& bind : action.bind) {
		if (bind == STEAM_INPUT) {
			//TODO
		}
		else if (bind == OPEN_XR) {
			//TODO
		}
		else if (bind == SDL_KEY) {
			//TODO
		}
		else if (bind == SDL_MOUSE) {
			//TODO
		}
		else if (bind == SDL_GAMEPAD) {
			//TODO
		}
	}
	return value ;
}

glm::vec2 InputPlugin::getVec2(int action_id){
	auto iter = actions.find(action_id);
	if (iter == actions.end()) {
		return glm::vec2();
	}

	const Action& action = iter->second;
	if (action.type != VEC2) {
		return glm::vec2();
	}
	glm::vec2 value = glm::vec2();
	for (const InputSource& bind : action.bind) {
		if (bind == STEAM_INPUT) {
			//TODO
		}
		else if (bind == OPEN_XR) {
			//TODO
		}
		else if (bind == SDL_KEY) {
			//TODO
		}
		else if (bind == SDL_MOUSE) {
			//TODO
		}
		else if (bind == SDL_GAMEPAD) {
			//TODO
		}
	}
	return value;
}

glm::vec3 InputPlugin::getVec3(int action_id){
	auto iter = actions.find(action_id);
	if (iter == actions.end()) {
		return glm::vec3();
	}

	Action& action = iter->second;
	if (action.type != VEC2) {
		return glm::vec3();
	}
	glm::vec3 value = glm::vec3();
	for (const InputSource& bind : action.bind) {
		if (bind == STEAM_INPUT) {
			//TODO
		}
		else if (bind == OPEN_XR) {
			//TODO
		}
		else if (bind == SDL_KEY) {
			//TODO
		}
		else if (bind == SDL_MOUSE) {
			//TODO
		}
		else if (bind == SDL_GAMEPAD) {
			//TODO
		}
	}
	return value;
}

glm::mat4 InputPlugin::getPose(int action_id){
	auto iter = actions.find(action_id);
	if (iter == actions.end()) {
		return glm::mat4();
	}

	Action& action = iter->second;
	if (action.type != POSE) {
		return glm::mat4();
	}
	glm::mat4 value = glm::mat4();
	for (const InputSource& bind : action.bind) {
		if (bind == STEAM_INPUT) {
			//TODO
		}
		else if (bind == OPEN_XR) {
			//TODO
		}
		else if (bind == SDL_KEY) {
			//TODO
		}
		else if (bind == SDL_MOUSE) {
			//TODO
		}
		else if (bind == SDL_GAMEPAD) {
			//TODO
		}
	}
	return value;
}