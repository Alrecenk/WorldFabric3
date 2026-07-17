#ifndef _SCENE_DEMO_APP2_H_
#define _SCENE_DEMO_APP2_H_ 1

#include "AsyncPlugin.h"
#include "MachineState.h"
#include "VulkanPlugin.h"
#include "glm/glm.hpp"
#include "GLTF.h"
#include <stdio.h>
#include <cstdlib>

#include <string>
#include <map>




class SceneDemoApp2 : public MachineState {

public:




	static inline const std::string state_name = "scene_demo_2_state";

	//Loads models from the hard drive on construction
	SceneDemoApp2();

	void run() override;

	// Called when switching into this sate before the first time run is claled
	void enter(std::shared_ptr<MachineState> from) override;


	// Called when switching outof this state after the last time run is called
	void exit(std::shared_ptr<MachineState> to) override;




private:


	//std::shared_ptr<TriangleShaderProgram> mesh_program;
	//std::shared_ptr<TriangleShaderProgram> shadow_program;
	//std::shared_ptr <ScreenShaderProgram>  ambient_program;
	//std::shared_ptr <ScreenShaderProgram>  light_program;
	//std::shared_ptr<ScreenModel<ScreenPushConstants, AmbientComponent>> ambient_post_effect;
	//std::shared_ptr<ScreenModel<ScreenPushConstants, LightComponent>> light_post_effect;
	int ambient_effect_id;
	int light_effect_id;

	std::chrono::high_resolution_clock::time_point last_run_time;
	std::chrono::high_resolution_clock::time_point current_time;

	int fox_id;
	int shop_id;
	int shop_window_id;
	glm::quat base_rotation;
	glm::quat target_rot;
	int mouse_particle_id;

	std::vector<int> cube_id;
	std::vector<glm::mat4> cube_rot;

	std::vector<int> moving_lights; 
	float current_angle = 0 ;

	//int shadow_resolution = 4096;
	//std::shared_ptr<RenderTarget> shadow_target;

};
#endif // #ifndef _SCENE_DEMO_APP_H_