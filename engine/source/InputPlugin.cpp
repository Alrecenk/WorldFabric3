#include "InputPlugin.h"

//Takes as input a path on disk to a json action file that matches the spec
InputPlugin::InputPlugin(const std::string input_action_file){
	action_file = Variant::loadJSONFile(input_action_file) ;
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
	//TODO
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
	for(InputSource& bind : action.bind){
		if(bind == STEAM_INPUT){
			//TODO
		}else if(bind == OPEN_XR){
			//TODO
		}else if(bind == SDL_KEY){
			//TODO
		}else if (bind == SDL_MOUSE) {
			//TODO
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
	for (InputSource& bind : action.bind) {
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

	Action& action = iter->second;
	if (action.type != VEC2) {
		return glm::vec2();
	}
	glm::vec2 value = glm::vec2();
	for (InputSource& bind : action.bind) {
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
	for (InputSource& bind : action.bind) {
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
	for (InputSource& bind : action.bind) {
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