#include "MirrorApp.h"
//#include "CursorPlugin.h"
#include "ConvexShape.h"
#include "LinearPredictiveCode.h"
#include "ParticlePlugin.h"

//Loads models from the hard drive on construction
MirrorApp::MirrorApp() {

}




// Called when switching into this sate before the first time run is claled
void MirrorApp::enter(std::shared_ptr<MachineState> from) {
	OpenXRPlugin* controls = getTool<OpenXRPlugin>() ;
	ScenePlugin* scene = getTool<ScenePlugin>();

	controls->setBackgroundColor(glm::vec3(0.0f, 0.0f, 0.0f));
	
	glm::mat4 current_head_pose = controls->getHeadPose();

	// Load any models we need that aren't there yet
	if (!scene->hasModel(avatar_model)) {
		scene->createModelSet(avatar_model, avatar_model_file, false);
		std::shared_ptr<GLTF> avatar = scene->getModelController(avatar_model);
		avatar->setToBasePose();
		initial_head_matrix = glm::mat4_cast(avatar->createPin(head, avatar->first_person_bone, avatar->first_person_offset, 1.0f, 0.1f));
		initial_left_hand_matrix = glm::inverse(glm::mat4_cast(avatar->createPin(left_hand, avatar->human_bone[left_hand], glm::vec3(0, 0, 0), 1.0f, 0.1f)));
		initial_right_hand_matrix = glm::inverse(glm::mat4_cast(avatar->createPin(right_hand, avatar->human_bone[right_hand], glm::vec3(0, 0, 0), 1.0f, 0.1f)));
		initial_hips_matrix = glm::inverse(glm::mat4_cast(avatar->createPin(hips, avatar->human_bone[hips], glm::vec3(0, 0, 0), 1.0f, 0.1f)));
		avatar->computeNodeMatrices();

		avatar->colliders[1].radius *= 1.3f ; // boost the collider size in the middle of the body to include the breasts for hair collision
		avatar->colliders[21].radius *= 1.5f; // make hand colliders bigger
		avatar->colliders[13].radius *= 1.5f; // make hand colliders bigger
		avatar->colliders[21].offset *= 2.5f ;// movre hand collider otu into palm
		avatar->colliders[13].offset *= 2.5f;
		avatar->nodes[avatar->human_bone["rightShoulder"]].stiffness = 3.0f;
		avatar->nodes[avatar->human_bone["leftShoulder"]].stiffness = 3.0f;

		std::shared_ptr<GLTF> mirror_image = avatar->createMirrorImage();
		scene->createModelSet(mirror_model, mirror_image, false);
	}

	if(!scene->hasModel(scene_model)){
		scene->createModelSet(scene_model, scene_model, false);
	}

	if (!scene->hasModel(small_box_model)) {
		std::shared_ptr<ConvexShape> small_box = std::make_shared<ConvexShape>(ConvexShape::makeAxisAlignedBox(glm::vec3(0.005f, 0.005f, 0.005f)));
		std::shared_ptr<GLTF> gltf_model = std::shared_ptr<GLTF>(new GLTF);
		gltf_model->setPolyhedronModel(small_box->vertex, small_box->face, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
		scene->createModelSet(small_box_model, gltf_model, false);
	}

	if (!scene->hasModel(small_sphere_model)) {
		std::shared_ptr<ConvexShape> small_sphere = std::make_shared<ConvexShape>(ConvexShape::makeSphere(glm::vec3(0,0,0),0.005f,2));
		std::shared_ptr<GLTF> gltf_model = std::shared_ptr<GLTF>(new GLTF);
		gltf_model->setPolyhedronModel(small_sphere->vertex, small_sphere->face, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
		scene->createModelSet(small_sphere_model, gltf_model, false);
	}

	std::shared_ptr<GLTF> avatar = scene->getModelController(avatar_model);

	// Avatar control
	if (!scene->hasInstance(my_instance)) {

		my_instance = scene->createInstance(avatar_model, avatar_pose);
		mirror_instance = scene->createInstance(mirror_model, avatar_pose);
		scene_instance = scene->createInstance(scene_model,scene_pose) ;
		camera_avatar_instance = scene->createInstance(avatar_model, avatar_pose);
		camera_avatar_instance_2 = scene->createInstance(mirror_model, avatar_pose);
		camera_scene_instance = scene->createInstance(scene_model, scene_pose);

		scene->createPin(my_instance, hips, avatar->human_bone[hips], glm::vec3(0, 0, 0), 2.0f, 2.0f);
		scene->createPin(my_instance, head, avatar->first_person_bone, avatar->first_person_offset, 1.0f, 1.0f);

		finger_map = std::vector<BoneMapping>();
		if(hand_tracking){ // GO into calibration mode
			scene->enableIK(my_instance, false);

			finger_map.emplace_back(left_hand_skeleton, "wrist_l", avatar->human_bone["leftHand"]);

			finger_map.emplace_back(left_hand_skeleton, "finger_thumb_0_l");
			finger_map.emplace_back(left_hand_skeleton, "finger_thumb_1_l", avatar->human_bone["leftThumbProximal"]);
			finger_map.emplace_back(left_hand_skeleton, "finger_thumb_2_l", avatar->human_bone["leftThumbIntermediate"]);
			finger_map.emplace_back(left_hand_skeleton, "finger_thumb_l_end", avatar->human_bone["leftThumbDistal"]);

			finger_map.emplace_back(left_hand_skeleton, "finger_index_0_l");
			finger_map.emplace_back(left_hand_skeleton, "finger_index_1_l", avatar->human_bone["leftIndexProximal"]);
			finger_map.emplace_back(left_hand_skeleton, "finger_index_2_l", avatar->human_bone["leftIndexIntermediate"]);
			finger_map.emplace_back(left_hand_skeleton, "finger_index_l_end", avatar->human_bone["leftIndexDistal"]);

			finger_map.emplace_back(left_hand_skeleton, "finger_middle_0_l");
			finger_map.emplace_back(left_hand_skeleton, "finger_middle_1_l", avatar->human_bone["leftMiddleProximal"]);
			finger_map.emplace_back(left_hand_skeleton, "finger_middle_2_l", avatar->human_bone["leftMiddleIntermediate"]);
			finger_map.emplace_back(left_hand_skeleton, "finger_middle_l_end", avatar->human_bone["leftMiddleDistal"]);

			finger_map.emplace_back(left_hand_skeleton, "finger_ring_0_l");
			finger_map.emplace_back(left_hand_skeleton, "finger_ring_1_l", avatar->human_bone["leftRingProximal"]);
			finger_map.emplace_back(left_hand_skeleton, "finger_ring_2_l", avatar->human_bone["leftRingIntermediate"]);
			finger_map.emplace_back(left_hand_skeleton, "finger_ring_l_end", avatar->human_bone["leftRingDistal"]);

			finger_map.emplace_back(left_hand_skeleton, "finger_pinky_0_l");
			finger_map.emplace_back(left_hand_skeleton, "finger_pinky_1_l", avatar->human_bone["leftLittleProximal"]);
			finger_map.emplace_back(left_hand_skeleton, "finger_pinky_2_l", avatar->human_bone["leftLittleIntermediate"]);
			finger_map.emplace_back(left_hand_skeleton, "finger_pinky_l_end", avatar->human_bone["leftLittleDistal"]);

			finger_map.emplace_back(right_hand_skeleton, "wrist_r", avatar->human_bone["rightHand"]);

			finger_map.emplace_back(right_hand_skeleton, "finger_thumb_0_r");
			finger_map.emplace_back(right_hand_skeleton, "finger_thumb_1_r", avatar->human_bone["rightThumbProximal"]);
			finger_map.emplace_back(right_hand_skeleton, "finger_thumb_2_r", avatar->human_bone["rightThumbIntermediate"]);
			finger_map.emplace_back(right_hand_skeleton, "finger_thumb_r_end", avatar->human_bone["rightThumbDistal"]);

			finger_map.emplace_back(right_hand_skeleton, "finger_index_0_r");
			finger_map.emplace_back(right_hand_skeleton, "finger_index_1_r", avatar->human_bone["rightIndexProximal"]);
			finger_map.emplace_back(right_hand_skeleton, "finger_index_2_r", avatar->human_bone["rightIndexIntermediate"]);
			finger_map.emplace_back(right_hand_skeleton, "finger_index_r_end", avatar->human_bone["rightIndexDistal"]);

			finger_map.emplace_back(right_hand_skeleton, "finger_middle_0_r");
			finger_map.emplace_back(right_hand_skeleton, "finger_middle_1_r", avatar->human_bone["rightMiddleProximal"]);
			finger_map.emplace_back(right_hand_skeleton, "finger_middle_2_r", avatar->human_bone["rightMiddleIntermediate"]);
			finger_map.emplace_back(right_hand_skeleton, "finger_middle_r_end", avatar->human_bone["rightMiddleDistal"]);

			finger_map.emplace_back(right_hand_skeleton, "finger_ring_0_r");
			finger_map.emplace_back(right_hand_skeleton, "finger_ring_1_r", avatar->human_bone["rightRingProximal"]);
			finger_map.emplace_back(right_hand_skeleton, "finger_ring_2_r", avatar->human_bone["rightRingIntermediate"]);
			finger_map.emplace_back(right_hand_skeleton, "finger_ring_r_end", avatar->human_bone["rightRingDistal"]);

			finger_map.emplace_back(right_hand_skeleton, "finger_pinky_0_r");
			finger_map.emplace_back(right_hand_skeleton, "finger_pinky_1_r", avatar->human_bone["rightLittleProximal"]);
			finger_map.emplace_back(right_hand_skeleton, "finger_pinky_2_r", avatar->human_bone["rightLittleIntermediate"]);
			finger_map.emplace_back(right_hand_skeleton, "finger_pinky_r_end", avatar->human_bone["rightLittleDistal"]);

		}else if(OpenXRPlugin::ENABLED){
			scene->createPin(my_instance, left_hand, avatar->human_bone[left_hand], glm::vec3(0, 0, 0), 1.0f, 1.0f);
			scene->createPin(my_instance, right_hand, avatar->human_bone[right_hand], glm::vec3(0, 0, 0), 1.0f, 1.0f);
			scene->enableIK(my_instance, true);
		}


	}

	printf("Shoulders: %d, %d\n", avatar->human_bone["rightShoulder"],avatar->human_bone["leftShoulder"]) ;

	
	if(OpenXRPlugin::ENABLED && hand_tracking){
		scene->overrideBoneOrientation(my_instance, avatar->human_bone["leftShoulder"], glm::rotate(glm::mat4(1.0f), 0.4f, glm::vec3(0, -1, 0)));
		scene->overrideBoneOrientation(my_instance, avatar->human_bone["rightShoulder"],glm::rotate(glm::mat4(1.0f),0.4f, glm::vec3(0,1,0)) );
	

		scene->overrideBoneOrientation(my_instance, avatar->human_bone["leftHand"], glm::rotate(glm::mat4(1.0f), 1.2f, glm::vec3(0, 0, 1)));
		scene->overrideBoneOrientation(my_instance, avatar->human_bone["rightHand"], glm::rotate(glm::mat4(1.0f), 1.2f, glm::vec3(0, 0, -1)));
	}

	calibrated = false;
	start_time = now();
	recenter(scene, current_head_pose);
}


// Called when switching outof this state after the last time run is called
void MirrorApp::exit(std::shared_ptr<MachineState> to) {
	ScenePlugin* scene = getTool<ScenePlugin>();


	std::shared_ptr<GLTF> avatar = scene->getModelController("avatar");

	scene->deleteInstance(my_instance);
	//scene->deleteInstance(mirror_instance);


}

// Called when switching into this sate before the first time run is claled
void MirrorApp::run() {

	OpenXRPlugin* controls = getTool<OpenXRPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	AudioPlugin* audio = getTool<AudioPlugin>();
	VulkanPlugin* window = getTool<VulkanPlugin>();
	double time = getTool<PanelPlugin>()->getTime();
	glm::mat4 current_head_pose = controls->getHeadPose();

	audio->SetListenerToHMD(current_head_pose); // set listener position to VR head location
	std::shared_ptr<GLTF> avatar = scene->getModelController(avatar_model);
	glm::mat4 final_avatar_pose = avatar_pose;
	if (wiggle_enabled) {
		double time = getTool<PanelPlugin>()->getTime();
		final_avatar_pose = glm::rotate(final_avatar_pose, (float)sin(time * 3.0f) * 0.3f, glm::vec3(1, 0, 1));
	}
	if(!calibrated && hand_tracking){
		scene->setPose(my_instance, avatar_pose * coord_fix);	
		if(millisBetween(start_time,now()) > millis_before_calibrate && OpenXRPlugin::ENABLED){
			for (auto& bone_map : finger_map) {
				bone_map.addCalibrationPoint(avatar_model, avatar_pose * coord_fix) ;
			}
			scene->disableOverrideBoneOrientation(my_instance, avatar->human_bone["rightShoulder"]);
			scene->disableOverrideBoneOrientation(my_instance, avatar->human_bone["leftShoulder"]);
			scene->disableOverrideBoneOrientation(my_instance, avatar->human_bone["rightHand"]);
			scene->disableOverrideBoneOrientation(my_instance, avatar->human_bone["leftHand"]);
			scene->enableIK(my_instance, true);
			
			calibrated = true ;
		}
	}else if(OpenXRPlugin::ENABLED){
		scene->setPose(my_instance, final_avatar_pose * coord_fix);
		scene->setPinTarget(my_instance, hips, avatar_pose * initial_hips_matrix * coord_fix);
		scene->setPinTarget(my_instance, head, current_head_pose * coord_fix);
		
		if(hand_tracking){
			std::map<int,glm::quat> finger_rotations ;
			for (auto& bone_map : finger_map) {
				if (bone_map.avatar_bone_index > 0) {
					bone_map.applyTracking(my_instance) ;
				}
			}
		}else{

			glm::mat4 current_left_hand_pose = controls->getPose("/actions/general/in/left_pose");
			glm::mat4 current_right_hand_pose = controls->getPose("/actions/general/in/right_pose");
		
			// Map controller axes in steamVR to correct axes for VRM hands
			glm::mat4 left_hand_pose_fix = { 0.0f,1.0f,0.0f,0.0f,
												-1.0f,0.0f,0.0f,0.0f,
												0.0f,0.0f,1.0f,0.0f,
												0.0f,0.0f,0.0f,1.0f
			};

			glm::mat4 right_hand_pose_fix = { 0.0f,-1.0f,0.0f,0.0f,
												1.0f,0.0f,0.0f,0.0f,
												0.0f,0.0f,1.0f,0.0f,
												0.0f,0.0f,0.0f,1.0f
			};

			current_left_hand_pose = current_left_hand_pose * left_hand_pose_fix * initial_left_hand_matrix;
			current_right_hand_pose = current_right_hand_pose * right_hand_pose_fix * initial_right_hand_matrix;
			
			scene->setPinTarget(my_instance, left_hand, current_left_hand_pose * coord_fix);
			scene->setPinTarget(my_instance, right_hand, current_right_hand_pose * coord_fix);			
		}
	
				
	}


	for (auto& bone_map : finger_map) {
		bone_map.setBoxes(avatar_model, avatar_pose * coord_fix) ;

	}

	
	scene->setPose(my_instance, final_avatar_pose * coord_fix) ;

	//Set the mirror pose to match but mirrored
	std::vector<glm::mat4> last_bone_data = scene->getBoneData(my_instance);
	if (last_bone_data.size() > 0) {
		glm::mat4 mirror_pose = glm::mat4(1.0f);
		mirror_pose = glm::translate(mirror_pose, glm::vec3(0, 0, -mirror_distance));
		mirror_pose = glm::scale(mirror_pose, glm::vec3(1, 1, -1));
		std::shared_ptr<GLTF> mirror = scene->getModelController(mirror_model);
		scene->setPose(mirror_instance, mirror_pose * final_avatar_pose * coord_fix, last_bone_data);

	}

	if (morph_weights.size() != avatar->morph_names.size()) {
		morph_weights = std::vector<float>(avatar->morph_names.size(), 0);
	}

	if (controls->getBoolean("/actions/general/in/press_y") || window->keyDown(SDLK_DOWN)) {
		if(!down_held){
			recenter(scene, current_head_pose);
		}
		down_held = true ;
	}else{
		down_held = false ;
	}

	// Check if escape pressed to exit
	if (window->getLastKeyPress() == SDLK_ESCAPE) {
		getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
	}

	if(window->keyDown(SDLK_SPACE) || controls->getBoolean("/actions/general/in/press_b")){
		if(!space_held){ // space pressed
			if(!recording){
				audio->startRecording("Vozard");
				recording = true ;
			}else{
				audio->stopRecording();

				printf("Number of samples recorded: %d\n", audio->getMicrophoneSessionSamples()) ;
				AudioPlugin::SoundData recorded_sound = audio->getMicrophoneSound(0,audio->getMicrophoneSessionSamples()) ;
				
				recording = false;
				//AudioPlugin::saveWAV(recorded_sound, "recorded.wav");

				AudioPlugin::SoundData synth(synthesized_voice,recorded_sound.sample_rate) ;
				//AudioPlugin::saveWAV(synth, "synthesized.wav");

				synthesized_voice.clear();
			}

		}
		space_held = true ;
	}else{
		if(space_held){

		}
		space_held = false; 
	}

	if (window->keyDown(SDLK_LEFT) && !left_held) { // left key press
		selected_morph--;
		if(selected_morph <0){
			selected_morph = 0 ;
		}
		printf("Morph Selected: %d -> %s\n", selected_morph, avatar->morph_names[selected_morph].c_str());
	}
	left_held = window->keyDown(SDLK_LEFT) ;

	if (window->keyDown(SDLK_RIGHT) && !right_held) { // left key press
		selected_morph++; 
		if (selected_morph > morph_weights.size()-1) {
			selected_morph = (int)morph_weights.size() - 1;
		}
		printf("Morph Selected: %d -> %s\n", selected_morph, avatar->morph_names[selected_morph].c_str());
	}
	right_held = window->keyDown(SDLK_RIGHT);

	

	if(recording){
		
		while(audio->getMicrophoneSessionSamples() - synthesized_voice.size() >= audio_samples_per_pose){


			AudioPlugin::SoundData small_sound_chunk = audio->getMicrophoneSound((int)synthesized_voice.size(), audio_samples_for_energy_check);
			std::vector<float> energy_check = small_sound_chunk.getNormalizedSignal();
			double raw_energy = LinearPredictiveCode::getEnergy(energy_check);

			AudioPlugin::SoundData sound_chunk = audio->getMicrophoneSound((int)synthesized_voice.size(),audio_samples_per_pose) ;
			std::vector<float> signal = sound_chunk.getNormalizedSignal();
	
			//(std::vector<float> raw, bool normalize = true, float pre_emphasis = 0.0f, bool hamming = false, float white_noise = 0.0f, int smooth_window_radius = 0)
			signal = LinearPredictiveCode::prepareWindow(signal, true, 0.65f, false, 0, 0) ;
			LinearPredictiveCode LPC(signal, sound_chunk.sample_rate, 20) ;

			signal = LPC.synthesizeVoice(120.0f, 0.02f) ;
			//signal = LinearPredictiveCode::prepareWindow(signal);
			synthesized_voice.insert(synthesized_voice.end(), signal.begin(),signal.begin() + audio_samples_per_pose) ;
			//printf("mic samples: %d, Signal size: %d\n", audio->getMicrophoneSessionSamples(), synthesized_voice.size());

			if(raw_energy <= 1e-3){ // quiet means mouth closed
				for (auto& [form, index] : formant_to_pose) {
					target_mouth_morph[index] = 0;
				}
			}else if(LPC.signal_energy > 2.0* LPC.residual_energy){
				//printf("Detected sound from %d samples.\n", audio_samples_per_pose) ;
				
				std::vector<ComplexNumber> poles = LPC.computeSoundPoles() ;
				glm::vec2 formants = LinearPredictiveCode::extractFormantFrequencies(poles, 48000);
				if(formants.x > 100 && formants.y < 5000){
					//printf("F1: %f, F2: %f\n", formants.x, formants.y);
					int best_index = -1;
					float best_distance = 1e6f;
					for (const std::pair<glm::vec2, int>& example : formant_to_pose) {
						float dist = LinearPredictiveCode::centDistance(formants, example.first, 48000);
						//printf("Distance for %d == %f\n", example.second, dist) ;
						if (dist < best_distance) {
							best_distance = dist;
							best_index = example.second;
						}
					}

					if(best_index >= 0){
						for (auto& [form, index] : formant_to_pose) {
							target_mouth_morph[index] = 0;
						}
						target_mouth_morph[best_index] = 1;
					}
				}
			}else{
				//printf("Raw Energy: %f Signal Energy: %f Residual Energy: %f Detected noise.\n", (float)raw_energy, (float)LPC.signal_energy, (float)LPC.residual_energy);
			}

		}

		float dt = (float)(time - last_morph_time);
		last_morph_time = time ;
		float step = dt * mouth_speed ;
		for (auto& [index, weight] : target_mouth_morph) {
			if(fabs (target_mouth_morph[index] - morph_weights[index]) < step){
				morph_weights[index] = target_mouth_morph[index] ;
			}else if(target_mouth_morph[index] > morph_weights[index]){
				morph_weights[index] += step ;
			}else{
				morph_weights[index] -= step;
			}
		}

	}else{
		morph_weights = std::vector<float>(avatar->morph_names.size(), 0);
		morph_weights[selected_morph] = 1.0f ;
	}

	updateBlink(morph_weights);
	//morph_weights[blink_morph] = randomFloat();
	scene->setMorphWeights(avatar_model, morph_weights);
	scene->setMorphWeights(mirror_model, morph_weights);
	
	pose_history.emplace_back(time, final_avatar_pose, last_bone_data);
	while(pose_history.front().time < time - pose_delay){
		pose_history.pop_front();
	}
	HistoryPose& h = pose_history.front() ;
	//printf("pose time : %f time : %f Delay : %f \n", h.time, time, time - h.time) ;
	glm::mat4 shift_pose = glm::mat4(1.0f);
	shift_pose = glm::translate(shift_pose, recording_offset);
	scene->setPose(camera_avatar_instance, shift_pose * h.pose * coord_fix, h.bone_data);
	scene->setPose(camera_avatar_instance_2, shift_pose * h.pose * coord_fix, h.bone_data);
	scene->setPose(camera_scene_instance, shift_pose * scene_pose) ;
	

	ParticlePlugin* particles = getTool<ParticlePlugin>();
	for (int& p : spring_debug_particles) {
		particles->destroyParticle(p);
	}
	spring_debug_particles.clear();


	std::vector<std::pair<glm::vec3, float>> spring_debug = scene->getSpringBoneWorldPositions(my_instance);

/*	
	for (std::pair<glm::vec3, float> sphere : spring_debug) {
		int p = particles->createParticle(0);
		spring_debug_particles.push_back(p);
		particles->setColor(p, glm::vec4(1, 0, 0, 0.5f));
		glm::mat4 pose = glm::mat4(1.0f);
		pose = glm::translate(pose, glm::vec3(glm::vec4(sphere.first, 1)));
		//printf("Sphere radius: %f pos: %f, %f,%f \n", sphere.second, sphere.first.x, sphere.first.y, sphere.first.z);
		pose = glm::scale(pose, glm::vec3(sphere.second*0.5f, sphere.second * 0.5f, sphere.second * 0.5f));
		particles->setPose(p, shift_pose* pose* coord_fix);

		//printf("spring pos: %f, %f, %f\n", pos.x, pos.y, pos.z);
	}
		
	float size = 0.02f ;
	std::vector<glm::vec3> spring_debug2 = scene->getSpringBoneTargetPositions(my_instance);
	for (glm::vec3 pos : spring_debug2) {
		int p = particles->createParticle(0);
		spring_debug_particles.push_back(p);
		particles->setColor(p, glm::vec4(0, 0, 1, 0.5f));
		glm::mat4 pose = glm::mat4(1.0f);
		//pos.z *= -1.0f; // TODO WHY? This seems wrong!
		pose = glm::translate(pose, glm::vec3(glm::vec4(pos, 1)));
		pose = glm::scale(pose, glm::vec3(size, size, size));
		particles->setPose(p, shift_pose* pose* coord_fix);
		//printf("spring pos: %f, %f, %f\n", pos.x, pos.y, pos.z);
	}
		
	std::vector<ScenePlugin::Capsule> spring_debug3 = scene->getSpringBoneColliders(my_instance);
	for (ScenePlugin::Capsule capsule : spring_debug3) {
		int p = particles->createParticle(0);
		spring_debug_particles.push_back(p);
		particles->setColor(p, glm::vec4(0, 0, 1, 0.5f));
		glm::mat4 pose = glm::mat4(1.0f);
		pose = glm::translate(pose, glm::vec3(glm::vec4(capsule.world_center.first, 1)));
		//printf("Sphere radius: %f pos: %f, %f,%f \n", sphere.world_radius, sphere.world_center.x, sphere.world_center.y, sphere.world_center.z);
		pose = glm::scale(pose, glm::vec3(capsule.world_radius, capsule.world_radius, capsule.world_radius));
		particles->setPose(p, shift_pose* pose* coord_fix);



		p = particles->createParticle(0);
		spring_debug_particles.push_back(p);
		particles->setColor(p, glm::vec4(0, 0, 1, 0.5f));
		pose = glm::mat4(1.0f);
		pose = glm::translate(pose, glm::vec3(glm::vec4(capsule.world_center.second, 1)));
		//printf("Sphere radius: %f pos: %f, %f,%f \n", sphere.world_radius, sphere.world_center.x, sphere.world_center.y, sphere.world_center.z);
		pose = glm::scale(pose, glm::vec3(capsule.world_radius, capsule.world_radius, capsule.world_radius));
		particles->setPose(p, shift_pose* pose* coord_fix);


		p = particles->createParticle(0);
		spring_debug_particles.push_back(p);
		particles->setColor(p, glm::vec4(0, 0, 1, 0.5f));
		pose = glm::mat4(1.0f);
		pose = glm::translate(pose, glm::vec3(glm::vec4((capsule.world_center.first + capsule.world_center.second) * 0.5f, 1)));
		//printf("Sphere radius: %f pos: %f, %f,%f \n", sphere.world_radius, sphere.world_center.x, sphere.world_center.y, sphere.world_center.z);
		pose = glm::scale(pose, glm::vec3(capsule.world_radius, capsule.world_radius, capsule.world_radius));
		particles->setPose(p, shift_pose* pose* coord_fix);

		//printf("spring pos: %f, %f, %f\n", pos.x, pos.y, pos.z);
	}
	*/


	

}


void MirrorApp::recenter(ScenePlugin* scene, glm::mat4& current_head_pose) {
	
	if (!OpenXRPlugin::ENABLED) {
		current_head_pose = glm::translate(glm::mat4(1.0f),glm::vec3(0,1,0));
	}

	printf("current_head_pose:\n");
	Variant(current_head_pose).printFormatted();

	std::shared_ptr<GLTF> avatar = scene->getModelController(avatar_model);

	avatar_pose = glm::mat4(1.0f);
	avatar->setToOriginalPose();
	printf("Avatar pose A:\n");
	Variant(avatar_pose).printFormatted();
	glm::vec4 model_head_position = avatar_pose * glm::vec4(avatar->getFirstPersonPosition(), 1.0f);

	// translate model pose so origin in camera pose lines up with head position
	glm::vec3 delta_head = glm::vec3(current_head_pose * glm::vec4(0, 0, 0, 1) - model_head_position);
	glm::mat4 translate(1.0f);
	translate = glm::translate(translate, delta_head);
	avatar_pose = translate * avatar_pose;
	printf("Avatar pose B:\n");
	Variant(avatar_pose).printFormatted();
	// rotate entire model around y axis so the body faces the looking direction of the head horizontally
	glm::dvec4 hz = current_head_pose * glm::vec4(0, 0, 1, 0);
	glm::dvec4 mz = avatar_pose * glm::vec4(0, 0, 1, 0);
	float angle = (float)acos((hz.x * mz.x + hz.z * mz.z) / sqrt((hz.x * hz.x + hz.z * hz.z) * (mz.x * mz.x + mz.z * mz.z)));
	if (angle < 0.001f || angle > 10.0f) {
		angle = 0.0f;
	}
	else if (hz.x * mz.z - hz.z * mz.x < 0) { // relevant component of cross to find direction to rotate
		angle *= -1;
	}
	printf("angle: %f\n", angle) ;
	avatar_pose = glm::rotate(avatar_pose, angle, glm::vec3(0.0f, 1.0f, 0.0f));
	printf("Avatar pose after recenter:\n") ;
	Variant(avatar_pose).printFormatted();

	// get the final model head pose
	model_head_position = avatar_pose * glm::vec4(avatar->getFirstPersonPosition(), 1.0f);

	glm::mat4 mirror_pose = glm::mat4(1.0f);
	mirror_pose = glm::translate(mirror_pose, glm::vec3(0, 0, -mirror_distance));
	mirror_pose = glm::scale(mirror_pose, glm::vec3(1, 1, -1));
	glm::vec3 look_at = mirror_pose*model_head_position ;

	VulkanPlugin* window = getTool<VulkanPlugin>();
	glm::vec3 camera_position = glm::vec3(model_head_position)*0.6f + look_at * 0.4f ;
	float fov = 0.8f;
	//window->window_target->setCamera(camera_position, look_at, fov, glm::vec3(0, 1, 0)); // look from head position at mirror

	look_at += recording_offset ;
	camera_position += recording_offset ;

	window->window_target->setCamera(look_at*0.7f + camera_position*0.3f, camera_position, fov, glm::vec3(0, 1, 0)); //from behind
	avatar->setToOriginalPose();
	scene->setPose(my_instance, avatar_pose,avatar->getBoneVector());
	scene->clearSpringBones(my_instance) ;
	scene->enableVRMSpringBones(my_instance, 10.0f);



	ScenePlugin::LightComponent lc;

	glm::vec3 light_pos = look_at*0.3f + camera_position * 0.7f + glm::vec3(-1,3,0);
	glm::vec3 light_look_at = look_at*0.2f + camera_position * 0.8f;

	int light_id = 0 ;
	lc.light_color = glm::vec4(.2, .2, .2, 1);
	scene->setLightComponent<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_id, lc) ;
	scene->moveLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_id,light_pos, light_look_at, glm::vec3(0, 1, 0.1), 0.7f, 20);

	
	
}


void MirrorApp::updateBlink(std::vector<float>& weights){
	double time = getTool<PanelPlugin>()->getTime();
	float dt = (float)(time - action_start_time) ;
	if(dt > action_length){
		blinking = !blinking ;
		action_start_time = time ;
		action_length = blinking ? blink_duration : blink_delay_min + randomFloat() * (blink_delay_max- blink_delay_min) ;
		//printf("Blinking: %d Time:%f\n", blinking ? 1: 0, action_length) ;
		dt = 0 ;
	}
	if(!blinking){
		weights[blink_morph] = 0 ;
	}else{
		weights[blink_morph] = 1.0f - fabs(dt-action_length*0.5f) / (action_length*0.5f) ;
		//weights[blink_morph] = 1.0f ;
	}
}
