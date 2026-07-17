#include "SceneDemoApp2.h"
#include "ParticlePlugin.h"
#include "ScenePlugin.h"
#include "Variant.h"
#include "FlagSet.h"



//Loads models from the hard drive on construction
SceneDemoApp2::SceneDemoApp2() {

}

// Called when switching into this sate before the first time run is claled
void SceneDemoApp2::enter(std::shared_ptr<MachineState> from) {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	OpenXRPlugin* xr = getTool<OpenXRPlugin>();

	scene->createModelSet("fox", "./assets/Fox2_base.glb", true);
	scene->addAnimation("tail_sway", "./assets/Fox2_tail_sway.glb", 0);
	scene->addAnimation("head_idle", "./assets/Fox2_head_idle.glb", 0);
	glm::mat4 fox_pose = glm::mat4(1.0f);
	//make some adjustments to bone stiffness for IK head tracking
	std::shared_ptr<GLTF> gltf_model = scene->getModelController("fox");
	gltf_model->setToBasePose();
	gltf_model->computeNodeMatrices();
	gltf_model->applyTransforms();
	int neck_bone = gltf_model->getBoneIndex("front_head");
	gltf_model->setStiffnessByDepth();
	gltf_model->nodes[neck_bone].stiffness = 20000; // make the head warp stiffer
	gltf_model->nodes[gltf_model->getBoneIndex("root")].stiffness = 1E6; // disallow root movement from IK

	scene->matchAnimationToModel("tail_sway", "fox"); // make sure animations are in the same transform space as the model
	scene->matchAnimationToModel("head_idle", "fox");

	//Make the fox

	
	//fox_pose = glm::rotate(fox_pose, 3.141f, glm::vec3(1, 0, 0));
	fox_pose = glm::translate(fox_pose, glm::vec3(2,0.3, 0));
	fox_pose = glm::rotate(fox_pose, 3.141f, glm::vec3(0, 1, 0));
	fox_id = scene->createInstance("fox", fox_pose);
	int tail_sway_id = scene->animateInstance(fox_id, "tail_sway", true);
	int head_idle_id = scene->animateInstance(fox_id, "head_idle", true);
	base_rotation = scene->createPin(fox_id, "neck", neck_bone, glm::vec3(0, 0, 0), 0.0f, 1.0f); // create a rotation IK pin on the head
	target_rot = base_rotation;
	scene->enableIK(fox_id, true);



	scene->createModelSet("shop", "./assets/shop.glb", false);
	glm::mat4 shop_pose = glm::mat4(1.0f);
	//beach_pose = glm::translate(beach_pose, glm::vec3(0, -0.50, 1));
	shop_pose = glm::rotate(shop_pose, 1.5f, glm::vec3(0, 1, 0));
	//shop_pose = glm::rotate(shop_pose, 3.141f, glm::vec3(1, 0, 0));
	//beach_pose = glm::scale(beach_pose, glm::vec3(5, 5, 5));
	shop_id = scene->createInstance("shop", shop_pose);


	scene->createModelSet("shop_window","./assets/shop_window.glb", false, true) ;
	shop_window_id = scene->createInstance("shop_window", shop_pose) ;

	//shop_pose = glm::rotate(shop_pose, 0.4f, glm::vec3(0, 1, 0));
	//int shop_window_id2 = scene->createInstance("shop_window", shop_pose);

	mouse_particle_id = particles->createParticle(0);
	particles->setColor(mouse_particle_id, glm::vec4(1, 0, 0, 1));
	particles->setPose(mouse_particle_id, glm::mat4(1.0f));



		float t = 0 ;

		ScenePlugin::LightComponent lc;
		
		glm::vec3 pos = glm::vec3(0, 2, -5) ;
		glm::vec3 look_at = glm::vec3(0, 0, 1);


		lc.light_color = glm::vec4(1, 0, 0, 1);
		moving_lights.push_back(scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(pos, look_at, glm::vec3(0, 1, 0), 0.55f, 30, 1024, 0, lc));

		lc.light_color = glm::vec4(1, 0, 1, 1);
		moving_lights.push_back(scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(pos, look_at, glm::vec3(0, 1, 0), 0.55f, 30, 1024, 0, lc));

		lc.light_color = glm::vec4(0, 1, 0, 1);
		moving_lights.push_back(scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(pos, look_at, glm::vec3(0, 1, 0), 0.55f, 30, 1024, 0, lc));

		lc.light_color = glm::vec4(1, 1, 0, 1);
		moving_lights.push_back(scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(pos, look_at, glm::vec3(0, 1, 0), 0.55f, 30, 1024, 0, lc));

		lc.light_color = glm::vec4(0, 0, 1, 1);
		moving_lights.push_back(scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(pos, look_at, glm::vec3(0, 1, 0), 0.55f, 30, 1024, 0, lc));

		lc.light_color = glm::vec4(0, 1, 1, 1);
		moving_lights.push_back(scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(pos, look_at, glm::vec3(0, 1, 0), 0.55f, 30, 1024, 0, lc));
}

void SceneDemoApp2::run() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	// Get the current time and time slice of frame
	current_time = now();
	float dt = microsBetween(last_run_time, current_time) / 1000000.0f;
	last_run_time = current_time;
	//printf("fps: %f\n", 1.0f / dt);

	// update the camera
	glm::vec3 P = { 0,5,-5 };
	glm::vec3 look_at = { 0,0,0 };
	float fov = 1.3f;




	window->window_target->setCamera(P, look_at, fov, glm::vec3(0, 1, 0));


	glm::vec3 look_at_position = window->window_target->camera_position + window->getMouseRay() * 3.0f;

	glm::mat4 particle_pose = glm::mat4(1.0f);
	particle_pose = glm::translate(particle_pose, look_at_position);
	particle_pose = glm::scale(particle_pose, glm::vec3(0.01, 0.01, 0.01));
	particles->setPose(mouse_particle_id, particle_pose);


	// get local coordinates of fox instance
	glm::vec3 current = scene->getPinPosition(fox_id, "neck");
	glm::mat4 current_pose = scene->getPose(fox_id);
	glm::vec3 forward = current_pose * glm::vec4(0, 0, 1.0f, 0.0f); // a forward looking vector
	forward = glm::normalize(forward);
	glm::vec3 right = current_pose * glm::vec4(1.0f, 0, 0, 0.0f);
	right = normalize(right);
	glm::vec3 up = current_pose * glm::vec4(0.0, 1.0f, 0, 0.0f);
	up = normalize(up);

	// compute angle to look at target
	glm::vec3 lookat_vec = look_at_position - current;
	lookat_vec = glm::normalize(lookat_vec);
	glm::quat pose_forward_rot = quatLookAt(-forward, up) * base_rotation;
	glm::quat new_target_rot = quatLookAt(-lookat_vec, up) * base_rotation;
	float angle = glm::angle(pose_forward_rot * glm::inverse(new_target_rot));


	target_rot = GLTF::slerp(target_rot, new_target_rot, 0.5); // general smoothing
	scene->setPinTarget(fox_id, "neck", target_rot);


	for (int k = 0; k < cube_id.size(); k++) {
		int id = cube_id[k];
		glm::mat4 cube_pose = scene->getPose(id);
		cube_pose = cube_rot[k] * cube_pose * cube_rot[k];
		scene->setPose(id, cube_pose);
	}


	current_angle+=dt ;
	for(int k=0;k<moving_lights.size();k++){

		glm::vec3 pos = glm::vec3(0, 2, -6);
		glm::vec3 look_at = glm::vec3(0, 0, 1);
		float a = current_angle + k * 6.28f/6.0f ;
		glm::vec3 add =glm::vec3(sin(a)*2, cos(a)*2,0);
		pos+= add ;
		look_at+=add;
		scene->moveLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(moving_lights[k],pos,look_at, add, 0.35f,30) ;
	}


	Uint32 which_pad = window->getLastGamepadPress().first ; // first is gamepad, second is button
	float left_x = window->getGamepadAxis(which_pad, SDL_GAMEPAD_AXIS_LEFTX);
	float left_y = window->getGamepadAxis(which_pad, SDL_GAMEPAD_AXIS_LEFTY);
	float right_y = window->getGamepadAxis(which_pad, SDL_GAMEPAD_AXIS_RIGHTY);
	glm::mat4 fox_pose = scene->getPose(fox_id);
	fox_pose = glm::translate(fox_pose, glm::vec3(left_x*dt, left_y*-dt,right_y*dt)) ;
	scene->setPose(fox_id, fox_pose) ;
	

	// Check if escape pressed to exit
	if (window->getLastKeyPress() == SDLK_ESCAPE) {
		getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
	}
}




// Called when switching out of this state after the last time run is called
void SceneDemoApp2::exit(std::shared_ptr<MachineState> to) {
	ScenePlugin* scene = getTool<ScenePlugin>();
	scene->deleteInstance(fox_id);
}