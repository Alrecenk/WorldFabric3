#ifndef _MIRROR_APP_H_
#define _MIRROR_APP_H_ 1

#include "AsyncPlugin.h"
#include "Variant.h"
#include "OpenXRPlugin.h"
#include "ScenePlugin.h"
#include "PanelPlugin.h"
#include "AudioPlugin.h"
#include "MachineState.h"
#include "glm/glm.hpp"

#include <stdio.h>
#include <cstdlib>

#include <string>
#include <map>


class MirrorApp : public MachineState {

public:


	static inline std::string state_name = "mirror_app";

	static inline int my_instance =  -1;
	static inline int mirror_instance = -6;
	static inline int scene_instance  ;

	static inline std::string avatar_model = "avatar";
	static inline std::string mirror_model = "mirror";
	static inline std::string avatar_model_file = "./assets/WFBunnyGirlloosewhiteshirt.vrm";
	static inline std::string scene_model = "./assets/table_scene.glb";

	static inline std::string small_box_model = "small_box";
	static inline std::string small_sphere_model = "small_sphere";

	static inline std::string head = "head";
	static inline std::string hips = "hips";
	static inline std::string left_hand= "leftHand";
	static inline std::string right_hand = "rightHand";
	static inline std::string left_hand_skeleton = "/actions/default/in/HandSkeletonLeft";
	static inline std::string right_hand_skeleton = "/actions/default/in/HandSkeletonRight";



	static inline bool hand_tracking = false;
	static inline bool calibrated = false;
	
	static inline float mirror_distance = 2.5f;
	static inline glm::mat4 scene_pose = glm::mat4(1.0f) ;

	glm::vec3 recording_offset = glm::vec3(10,0,0);
	double pose_delay = 1.3 ; // amount of time to delay poses to sync up with audio and morph data (this is needed when using a slow voice changer so movement matches audio)
	int camera_scene_instance;
	int camera_avatar_instance ;
	int camera_avatar_instance_2 ; // a mirroed version in case it isn't built for backface culling

	struct HistoryPose{
		double time  = - 1; 
		glm::mat4 pose ;
		std::vector<glm::mat4> bone_data ;
	};

	std::deque<HistoryPose> pose_history ;
	
	
	float blink_delay_min = 2.0f ;
	float blink_delay_max = 4.0f;
	float blink_duration = 0.1f ;
	bool blinking = false;
	double action_start_time = 0 ;
	float action_length = 1.0f ;
	int blink_morph = 13 ;
	int selected_morph = 0 ;


	bool space_held = false;
	bool left_held = false;
	bool right_held = false;
	bool down_held = false ;
	bool recording = false;
	float sample_duration = 0.04f ;
	int audio_samples_per_pose = (int)(48000*sample_duration) ;
	int audio_samples_for_energy_check = (48000 * 15) / 1000 ;
	int next_audio_sample = 0 ;
	std::vector<float> synthesized_voice ;
	


	//Formant F1+F2 frequencies to mouth pose table
	
/*
* This is a theoretical standard tuning, but it doesn't work great.
	std::unordered_map<glm::vec2, int> formant_to_pose = {
		{ {700, 1200}, 39}, // aa
		{ {400, 1900}, 40}, // ih
		{ {300, 700}, 41}, // ou
		{ {300, 2200}, 42}, // ee
		{ {400, 800}, 43} // oh
	} ;
*/

	//These are tuned for the avatar voice used in World Fabric videos
	std::unordered_map<glm::vec2, int> formant_to_pose = {
				{ {575, 2150}, 39}, // aa
				{ {415, 2750}, 40}, // I
				{ {350, 3850}, 41}, // U
				{ {425, 3750}, 42}, // E
				{ {455, 4200}, 43} // O
	};




	std::vector<float> morph_weights ;
	std::map<int, float> target_mouth_morph;
	double last_morph_time = 0;
	float mouth_speed = 5.0f; // unit morphs per second
	

	// Maps openXR skeleton and bone name to avatar bone name for the finger tips
	//std::map < std::pair < std::string, std::string>, std::string > finger_tips;


	class BoneMapping {
	public:
		std::string skeleton_name;
		std::string skeleton_bone_name;
		int avatar_bone_index = -1;
		int instance_id = 0;
		int instance_id_2 = 0 ;

		std::vector<glm::mat4> tracking ; // tracking position from calibration
		std::vector<glm::mat4> target ; //target position from calibration

		glm::quat pin_start_orientation ;


		static inline int next_id = 20000;
		BoneMapping(const std::string& skeleton, const std::string& bone, int index = -1){
			skeleton_name = skeleton ;
			skeleton_bone_name = bone ;
			avatar_bone_index = index;
			ScenePlugin* scene = getTool<ScenePlugin>();
			instance_id = scene->createInstance(small_box_model, glm::mat4(1.0f));
			instance_id_2 = scene->createInstance(small_sphere_model, glm::mat4(1.0f));
			if (avatar_bone_index >= 0) {
				pin_start_orientation = scene->createPin(my_instance, skeleton_bone_name, avatar_bone_index, glm::vec3(0, 0, 0), 1.0f, 1.0f);
			}
		
		}

		void setBoxes(const std::string& avatar_model, glm::mat4 instance_pose){
			glm::mat4 avatar_finger_pose(0);
			glm::mat4 finger_pose(0) ;
			ScenePlugin* scene = getTool<ScenePlugin>();
			if(!calibrated){

				OpenXRPlugin* controls = getTool<OpenXRPlugin>();
				
				finger_pose = controls->getSkeletonBoneTransform(skeleton_name, skeleton_bone_name);
				
				if(avatar_bone_index >= 0){
					std::shared_ptr<GLTF> avatar = scene->getModelController(avatar_model);
					avatar_finger_pose = instance_pose * avatar->nodes[avatar_bone_index].bone_to_model ;
				}
			}

			scene->setPose(instance_id, finger_pose);
			scene->setPose(instance_id_2, avatar_finger_pose);

		}

		void addCalibrationPoint(const std::string& avatar_model, glm::mat4 instance_pose ){
			if(avatar_bone_index == -1){
				return ;
			}
			OpenXRPlugin* controls = getTool<OpenXRPlugin>();
			ScenePlugin* scene = getTool<ScenePlugin>();
			std::shared_ptr<GLTF> avatar = scene->getModelController(avatar_model);

			//absolute position of tracking and 
			tracking.push_back( controls->getSkeletonBoneTransform(skeleton_name, skeleton_bone_name));
			target.push_back(instance_pose * avatar->nodes[avatar_bone_index].bone_to_model) ;


		}



		void applyTracking(int avatar_instance) {
			if (tracking.size() == 0 || avatar_bone_index == -1) {
				return ;
			}
			OpenXRPlugin* controls = getTool<OpenXRPlugin>();
			ScenePlugin* scene = getTool<ScenePlugin>();
			glm::mat4 tracked = controls->getSkeletonBoneTransform(skeleton_name, skeleton_bone_name);
			
			glm::mat4 scene_transform = tracked * glm::inverse(tracking[0]) * target[0] ;
			scene->setPinTarget(my_instance, skeleton_bone_name, scene_transform );
			
			
			//scene->setPose(instance_id_2, scene_transform);


		}

	};

	std::vector<BoneMapping> finger_map;



	//Loads models from the hard drive on construction
	MirrorApp();

	void run() override;

	// Called when switching into this sate before the first time run is claled
	void enter( std::shared_ptr<MachineState> from) override;


	// Called when switching outof this state after the last time run is called
	void exit(std::shared_ptr<MachineState> to) override;

	void updateBlink(std::vector<float>& weights);

private:
	glm::mat4 initial_head_matrix;
	glm::mat4 initial_left_hand_matrix;
	glm::mat4 initial_right_hand_matrix;
	glm::mat4 initial_hips_matrix;
	glm::mat4 avatar_pose;

	bool wiggle_enabled = true;


	std::chrono::high_resolution_clock::time_point start_time ;
	int millis_before_calibrate = 6000 ;

	std::vector<int> spring_debug_particles ;



	// Map controller axes in steamVR to correct axes for VRM hands
	static inline glm::mat4 left_hand_pose_fix = { 0.0f,1.0f,0.0f,0.0f,
										-1.0f,0.0f,0.0f,0.0f,
										0.0f,0.0f,1.0f,0.0f,
										0.0f,0.0f,0.0f,1.0f
	};

	static inline glm::mat4 right_hand_pose_fix = { 0.0f,-1.0f,0.0f,0.0f,
										1.0f,0.0f,0.0f,0.0f,
										0.0f,0.0f,1.0f,0.0f,
										0.0f,0.0f,0.0f,1.0f
	};

	static inline glm::mat4 coord_fix = glm::scale(glm::mat4(1.0), glm::vec3(-1, 1, -1));


	void recenter(ScenePlugin* scene, glm::mat4& current_head_pose);


};
#endif // #ifndef _MIRROR_APP_H_