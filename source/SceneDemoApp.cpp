#include "SceneDemoApp.h"
#include "ParticlePlugin.h"
#include "ScenePlugin.h"
#include "Variant.h"
#include "FlagSet.h"
#include "Registry.h"
#include "PanelPlugin.h"
#include "AudioPlugin.h"


//Loads models from the hard drive on construction
SceneDemoApp::SceneDemoApp(){

}

// Called when switching into this sate before the first time run is claled
void SceneDemoApp::enter(std::shared_ptr<MachineState> from) {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	OpenXRPlugin* xr = getTool<OpenXRPlugin>();
	AudioPlugin* sound = getTool<AudioPlugin>();

	scene->createModelSet("fox", "./assets/Fox2_base.glb", true) ;
	scene->addAnimation("tail_sway", "./assets/Fox2_tail_sway.glb",0);
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

	scene->matchAnimationToModel("tail_sway","fox"); // make sure animations are in the same transform space as the model
	scene->matchAnimationToModel("head_idle", "fox");

	//Make the fox
	fox_pose = glm::translate(fox_pose, glm::vec3(0, 0, 1));
	//fox_pose = glm::rotate(fox_pose, 3.141f, glm::vec3(1, 0, 0));
	fox_pose = glm::rotate(fox_pose, 3.1f, glm::vec3(0, 1, 0));
	fox_id = scene->createInstance("fox", fox_pose);
	int tail_sway_id = scene->animateInstance(fox_id, "tail_sway", true);
	int head_idle_id = scene->animateInstance(fox_id, "head_idle", true);
	base_rotation = scene->createPin(fox_id, "neck", neck_bone, glm::vec3(0, 0, 0), 0.0f, 1.0f); // create a rotation IK pin on the head
	target_rot = base_rotation;
	scene->enableIK(fox_id, true);


	
	scene->createModelSet("beach", "./Narball/asset/palmdiorama.glb", true);
	glm::mat4 beach_pose = glm::mat4(1.0f);
	beach_pose = glm::translate(beach_pose, glm::vec3(0,0.435,1));
	//beach_pose = glm::rotate(beach_pose, 3.141f, glm::vec3(0, 1, 0));
	//beach_pose = glm::rotate(beach_pose, 3.141f, glm::vec3(1, 0, 0));
	beach_pose = glm::scale(beach_pose,glm::vec3(5,5,5));
	beach_id = scene->createInstance("beach", beach_pose);
	

	
	mouse_particle_id = particles->createParticle(0);
	particles->setColor(mouse_particle_id, glm::vec4(1,0,0,1)) ;
	particles->setPose(mouse_particle_id,glm::mat4(1.0f));


	scene->createModelSet("cube", "./assets/bunny350.glb", true);
	
	int num_boxes = 100;
	for(int k=0;k<num_boxes;k++){
		
		glm::mat4 cube_pose = glm::mat4(1.0f);
		cube_pose = glm::translate(cube_pose, glm::vec3((randomFloat()-0.5)*5, (randomFloat()-0.5) * 5, (randomFloat() - 0.5) * 5));
		cube_pose = glm::rotate(cube_pose, randomFloat()*3.141f, glm::vec3(0, 1, 0));
		cube_pose = glm::rotate(cube_pose, randomFloat()*3.141f, glm::vec3(1, 0, 0));
		float size = (randomFloat()+1)*0.05f ;
		cube_pose = glm::scale(cube_pose, glm::vec3(size, size, size));

		cube_rot.push_back(glm::rotate(glm::mat4(1.0f), randomFloat() * 0.001f, glm::vec3(randomFloat()-0.5f, randomFloat() - 0.5f, randomFloat() - 0.5f)));
		cube_id.push_back( scene->createInstance("cube", cube_pose));
	}

	
	glm::vec3 pos = glm::vec3(0, 16, 10);
	glm::vec3 look_at = glm::vec3(0, 0, 0);

	ScenePlugin::LightComponent lc ;
	lc.light_color = glm::vec4(1, 1, 1, 1);
	scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(pos, look_at, glm::vec3(0, 1, 0), 0.6f, 30, 2048, 0, lc);


	tap_sound = sound->addWAV("./assets/tap.wav",1) ;
	ding_sound = sound->addWAV("./assets/goodding1.wav", 1);
	dings_sound = sound->addWAV("./assets/goodding3.wav", 1);
	sound->setGroupVolume(1,1) ;
	//sound->play("dings", glm::vec3(0,0,0));
	openMenu();

}

void SceneDemoApp::run(){
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	PanelPlugin* panels = getTool<PanelPlugin>();
	AudioPlugin* sound = getTool<AudioPlugin>();

	// Get the current time and time slice of frame
	current_time = now();
	float dt = microsBetween(last_run_time, current_time) / 1000000.0f;
	last_run_time = current_time;
	//printf("fps: %f\n", 1.0f/dt);

	// update the camera
	glm::vec3 P = { -1,2,-3 };
	glm::vec3 look_at = {0,0,1} ;
	float fov = 1.3f;


	window->window_target->setCamera(P,look_at, fov, glm::vec3(0, 1, 0)) ;

	glm::vec3 mouse_ray = window->getMouseRay() ;
	glm::vec3 look_at_position = window->window_target->camera_position  + mouse_ray * 2.0f;

	glm::mat4 particle_pose = glm::mat4(1.0f);
	particle_pose = glm::translate(particle_pose,look_at_position);
	particle_pose = glm::scale(particle_pose, glm::vec3(0.01, 0.01, 0.01));
	particles->setPose(mouse_particle_id, particle_pose);

	panels->setPointerByRay(window->window_target->camera_position, mouse_ray) ;
	


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
	
	
	for(int k=0;k<cube_id.size();k++){
		int id = cube_id[k];
		glm::mat4 cube_pose = scene->getPose(id) ;
		cube_pose = cube_rot[k]*cube_pose*cube_rot[k];
		scene->setPose(id, cube_pose) ;
	}
	

	if(ding_time.size() > next_ding && panels->getTime() >  ding_time[next_ding]){
		if(next_ding == 0 ){
			sound->play(tap_sound, glm::vec3(0, 0, 0));
		}else if(next_ding ==ding_time.size() -1){
			sound->play(dings_sound, glm::vec3(0, 0, 0));
		}else{
			sound->play(ding_sound, glm::vec3(0, 0, 0));
		}
		next_ding++;
	}


	// Check if escape pressed to exit
	if (window->getLastKeyPress() == SDLK_ESCAPE) {
		getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
	}
}




// Called when switching out of this state after the last time run is called
void SceneDemoApp::exit(std::shared_ptr<MachineState> to){
	ScenePlugin* scene = getTool<ScenePlugin>();
	scene->deleteInstance(fox_id);
}


void SceneDemoApp::openMenu(){
	VulkanPlugin* window = getTool<VulkanPlugin>();
	glm::vec3 camera_p = { -1,2,-3 };
	glm::vec3 camera_look_at = { 0,0,1 };
	glm::vec3 z = camera_look_at - camera_p;
	glm::vec3 y = glm::vec3(0, 1, 0);
	glm::vec3 x = glm::cross(z, y);
	y = glm::cross(x, z);
	z = glm::normalize(z) * 0.5f;
	x = glm::normalize(x) * 0.25f;
	y = glm::normalize(y) * 0.25f;

	PanelPlugin* panels = getTool<PanelPlugin>();
	int menu_panel_id = panels->createPanel(1024, 1024, { 0.8,0.8,1,0.5 });

	double open_time = panels->getTime();
	PanelPlugin::DefaultInstance initial;
	initial.pose = panels->getPose(camera_p + z - x * 0.1f - y * 0.1f, -y * 0.2f, x * 0.2f);
	panels->addPanelKeyFrame(menu_panel_id, open_time, initial);

	PanelPlugin::DefaultInstance result;
	result.pose = panels->getPose(camera_p + z - x - y, x * 2.0f, y * 2.0f);
	panels->addPanelKeyFrame(menu_panel_id, open_time + 0.5, result);

	ding_time.push_back(open_time + 0.25);
	

	int text_center_x = 750;
	std::vector<std::string> buttons  = { "Resume", "Restart", "Pangolins", "Options", "Exit"} ;
	
	int k = 0 ;
	double time = open_time + 1.0 ;
	for(auto& text : buttons){
		int element_id = panels->createElement(menu_panel_id);
		elements[element_id] = text ;
		std::shared_ptr<WFImage> element_image = panels->createTextImage(text, "arial", glm::vec4(0.1, 0, 0.1, 1.0), 9999);
		panels->setElementTexture(menu_panel_id, element_id, element_image);
		panels->setElementPosition(menu_panel_id, element_id, glm::vec2(text_center_x - element_image->getWidth() * 0.5f, 100 + k*150), (float)element_image->getWidth(), (float)element_image->getHeight());
		k++;

		glm::mat4 start_position = PanelPlugin::getElementPose(glm::vec2(text_center_x - element_image->getWidth() * 0.001f, 150 + k * 150), (float)element_image->getWidth() * 0.002f, (float)element_image->getHeight(), k);
		glm::mat4 final_position = PanelPlugin::getElementPose(glm::vec2(text_center_x - element_image->getWidth() * 0.5f, 0 + k * 150), (float)element_image->getWidth(), (float)element_image->getHeight(), k);
		
		final_position *= glm::rotate(glm::mat4(1),0.05f, glm::vec3(0, 0, 1));

		PanelPlugin::DefaultInstance inst;
		inst.pose = start_position;
		panels->addPanelElementKeyFrame(menu_panel_id, element_id, time, inst);
		inst.pose = final_position;
		time += 0.15;
		panels->addPanelElementKeyFrame(menu_panel_id, element_id, time, inst);
		ding_time.push_back(time);
		time += 0.3;
		element_base_pose[element_id] = final_position ;
		element_hover_pose[element_id] = PanelPlugin::getElementPose(glm::vec2(text_center_x - element_image->getWidth() * 0.5f -10, -10 + k * 150), 
			(float)element_image->getWidth() + 20, 
			(float)element_image->getHeight() + 20, k);

		element_hover_pose[element_id] *= glm::rotate(glm::mat4(1), -0.05f, glm::vec3(0,0,1)) ;
	}

	
	int image_element = panels->createElement(menu_panel_id);
	std::shared_ptr<WFImage> test_texture = window->loadImageFromFile("./assets/otterhat.png");
	panels->setElementTexture(menu_panel_id, image_element, test_texture);
	panels->setElementPosition(menu_panel_id, image_element, glm::vec2(10, 400), (float)test_texture->getWidth(), (float)test_texture->getHeight());


	glm::mat4 image_pose = PanelPlugin::getElementPose(glm::vec2(10, 400), (float)test_texture->getWidth(), (float)test_texture->getHeight(), 0);
	glm::mat4 spin = glm::translate(glm::mat4(1), glm::vec3(test_texture->getWidth() * 0.5f, 400+test_texture->getHeight() * 0.5f, 0))
		* glm::rotate(glm::mat4(1), 0.1f,glm::vec3(0,0,1))
		* glm::translate(glm::mat4(1), glm::vec3(test_texture->getWidth() * -0.5f, -400+test_texture->getHeight() * -0.5f, 0))
	;
	//time-=1.0f ;
	element_hover_pose[image_element] = image_pose ;
	for(int k=0;k<60;k++){
		PanelPlugin::DefaultInstance inst;
		inst.pose = image_pose;
		panels->addPanelElementKeyFrame(menu_panel_id, image_element, time, inst);
		image_pose = spin*image_pose  ;
		element_base_pose[image_element] = image_pose;
		time += 0.01;
	}

	ding_time.push_back(time);
	
	
	
	panels->setListener(this) ;

}


void SceneDemoApp::enterPanel(int panel){
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	particles->setColor(mouse_particle_id, glm::vec4(0, 0, 1, 1));


}

void SceneDemoApp::exitPanel(int panel){
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	particles->setColor(mouse_particle_id, glm::vec4(1, 0, 0, 1));
}

void SceneDemoApp::enterPanelElement(int panel, int element){
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	PanelPlugin* panels = getTool<PanelPlugin>();
	AudioPlugin* sound = getTool<AudioPlugin>();
	if (element >= 0) {
		particles->setColor(mouse_particle_id, glm::vec4(0, 1, 0, 1));
		PanelPlugin::DefaultInstance inst = panels->getPanelElementInstance<PanelPlugin::DefaultInstance>(panel, element) ;
		panels->clearKeyFrames(panel, element);
		panels->addPanelElementKeyFrame(panel, element, panels->getTime(), inst);
		inst.pose = element_hover_pose[element];
		panels->addPanelElementKeyFrame(panel, element, panels->getTime()+0.25,inst) ;
		sound->play(tap_sound,glm::vec3(0,0,0));
	}
	else if (panel >= 0) {
		particles->setColor(mouse_particle_id, glm::vec4(0, 0, 1, 1));
	}
	else {
		particles->setColor(mouse_particle_id, glm::vec4(1, 0, 0, 1));
	}
}

void SceneDemoApp::exitPanelElement(int panel, int element){
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	PanelPlugin* panels = getTool<PanelPlugin>();
	particles->setColor(mouse_particle_id, glm::vec4(0, 0, 1, 1));

	if (element >= 0) {
		PanelPlugin::DefaultInstance inst = panels->getPanelElementInstance<PanelPlugin::DefaultInstance>(panel, element);
		panels->clearKeyFrames(panel, element);
		panels->addPanelElementKeyFrame(panel, element, panels->getTime(), inst);
		inst.pose = element_base_pose[element];
		panels->addPanelElementKeyFrame(panel, element, panels->getTime() + 0.25, inst);

	}
}

void SceneDemoApp::pressPanel(int panel, int element, int button){

}

void SceneDemoApp::releasePanel(int panel, int element, int button){

}