#include "BallThrowApp.h"
#include "ParticlePlugin.h"
#include "ScenePlugin.h"
#include "Variant.h"
#include "FlagSet.h"



//Loads models from the hard drive on construction
BallThrowApp::BallThrowApp() {

}

// Called when switching into this sate before the first time run is claled
void BallThrowApp::enter(std::shared_ptr<MachineState> from) {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	particles = getTool<ParticlePlugin>();
	OpenXRPlugin* xr = getTool<OpenXRPlugin>();

	scene->createModelSet("fox", "./assets/Fox2_base.glb", true);
	scene->createModelSet("glasses", "./assets/Fox2_glasses.glb", true);
	scene->createModelSet("hat", "./assets/Fox2_top_hat.glb", true);
	scene->addAnimation("tail_sway", "./assets/Fox2_tail_sway.glb", 0);
	scene->addAnimation("head_idle", "./assets/Fox2_head_idle.glb", 0);
	scene->addAnimation("ball_catch", "./assets/Fox2_ball_catch.glb", 0);
	scene->addAnimation("ball_throw", "./assets/Fox2_ball_throw.glb", 0);

	
	//make some adjustments to bone stiffness for IK head tracking
	std::shared_ptr<GLTF> gltf_model = scene->getModelController("fox");
	gltf_model->setToBasePose();
	gltf_model->computeNodeMatrices();
	gltf_model->applyTransforms();
	FoxSequence::neck_bone = gltf_model->getBoneIndex("front_head");
	gltf_model->setStiffnessByDepth();
	gltf_model->nodes[FoxSequence::neck_bone].stiffness = 20000; // make the head warp stiffer
	gltf_model->nodes[gltf_model->getBoneIndex("root")].stiffness = 1E6; // disallow root movement from IK

	scene->matchAnimationToModel("tail_sway", "fox"); // make sure animations are in the same transform space as the model
	scene->matchAnimationToModel("head_idle", "fox");
	scene->matchAnimationToModel("ball_catch", "fox");
	scene->matchAnimationToModel("ball_throw", "fox");

	//Make the foxes

	glm::mat4 fox_pose = glm::mat4(1.0f);
	fox_pose = glm::translate(fox_pose, glm::vec3(4, 1.0, 0));
	fox_pose = glm::rotate(fox_pose, -3.141f / 2.0f, glm::vec3(0, 1, 0));
	foxes[0] = std::make_shared<FoxSequence>(FoxSequence(fox_pose, scene)) ;
	
	fox_pose = glm::mat4(1.0f);
	fox_pose = glm::translate(fox_pose, glm::vec3(-4, 1.0, 0));
	fox_pose = glm::rotate(fox_pose, 3.141f / 2.0f, glm::vec3(0, 1, 0));
	foxes[1] = std::make_shared<FoxSequence>(FoxSequence(fox_pose, scene));

	 fox_pose = glm::mat4(1.0f);
	fox_pose = glm::translate(fox_pose, glm::vec3(4, -2.0, 0));
	fox_pose = glm::rotate(fox_pose, -3.141f / 2.0f, glm::vec3(0, 1, 0));
	foxes[2] = std::make_shared<FoxSequence>(FoxSequence(fox_pose, scene));

	fox_pose = glm::mat4(1.0f);
	fox_pose = glm::translate(fox_pose, glm::vec3(-4, -2.0, 0));
	fox_pose = glm::rotate(fox_pose, 3.141f / 2.0f, glm::vec3(0, 1, 0));
	foxes[3] = std::make_shared<FoxSequence>(FoxSequence(fox_pose, scene));
	

	scene->addModelToInstance(foxes[0]->scene_id,"glasses");
	scene->addModelToInstance(foxes[3]->scene_id, "glasses");
	scene->addModelToInstance(foxes[0]->scene_id, "hat");
	scene->addModelToInstance(foxes[2]->scene_id, "hat");

	//make the balls
	scene->createModelSet("ball", "./Narball/asset/BeachBall.glb", true);

	balls[0] = std::make_shared<BallSequence>(BallSequence(scene)) ;
	balls[1] = std::make_shared<BallSequence>(BallSequence(scene));

	balls[0]->queue(0, foxes[0]->getHeld());
	balls[1]->queue(0, foxes[2]->getHeld());


	

	float start_time = 2.0f ;
	float hold_time = 1.0f ;
	float air_time = 3.0f ;
	//float ping = 0.3f ;
	float ping = 2.0f;
	while(start_time < 120.0f){
		//createDefaultSequence(start_time, 1.0f, 3.0f) ;
		//createServerStateSequence(start_time,hold_time,air_time,ping) ;
		//createOwnerSequence(start_time, hold_time, air_time, ping);
		//createRollbackSequence(start_time, hold_time, air_time, ping);
		//createRollbackInterpolationSequence(start_time, hold_time, air_time, ping, ping*0.35f);
		createTimeWarpSequence(start_time, hold_time, air_time, ping, 8.0f/(ping*0.5f));


		start_time += hold_time*2 + air_time*2 + ping*2.0f ; ;
	}

	

	/*
	glm::mat4 ball_pose = glm::mat4(1.0f);
	ball_pose =glm::scale(ball_pose,glm::vec3(ball_scale, ball_scale, ball_scale)) ;
	ball_id = scene->createInstance("ball", ball_pose);
*/

	// Make a nice ring light so it's extra smooth
	ScenePlugin::LightComponent lc;
	lc.light_color = glm::vec4(0.05, 0.05, 0.05 -20.0, 1);
	
	glm::vec3 look_at = glm::vec3(0, 0, 0);

	//createLight(const glm::vec3 & position, const glm::vec3 & look_at, const glm::vec3 & up, float fov, float far, int resolution, int transform_group, ComputeComponent first_component) {
	double radius = 2 ;
	for(double a = 0; a < 6.28;a+=0.6){
		glm::vec3 pos = glm::vec3(sin(a)*radius, 25, cos(a)*radius);
		moving_lights.push_back(scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(pos, look_at, glm::vec3(0, 1, 0), 0.55f, 30, 2048, 0, lc));
	}

	last_run_time = now();
	absolute_time = 0 ;
}

void BallThrowApp::run() {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	// Get the current time and time slice of frame
	current_time = now();
	float dt = microsBetween(last_run_time, current_time) / 1000000.0f;
	absolute_time += dt;
	last_run_time = current_time;
	//printf("fps: %f\n", 1.0f / dt);

	// update the camera
	glm::vec3 P = { 0,1.0,-25 };
	glm::vec3 look_at = { 0,0,0 };
	float fov = 0.3f;

	window->window_target->setCamera(P, look_at, fov, glm::vec3(0, 1, 0));




	

	glm::vec3 m_ray = window->getMouseRay() ;
	glm::vec3 c_pos = window->window_target->camera_position ;
	float s = -c_pos.z/m_ray.z ;
	glm::vec3 m_pos =  c_pos + m_ray * s ;


	if(window->mouseDown(1)){
		printf("mouse_pos: %f, %f, %f time: %lf\n", m_pos.x, m_pos.y, m_pos.z, absolute_time) ;
	}

	// Check if escape pressed to exit
	if (window->getLastKeyPress() == SDLK_ESCAPE) {
		getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
	}

	
		glm::vec3 ball_position = balls[0]->animate((float)absolute_time, scene);
		foxes[0]->lookAt(ball_position, scene);
		foxes[1]->lookAt(ball_position, scene);
		ball_position = balls[1]->animate((float)absolute_time, scene);
		foxes[2]->lookAt(ball_position, scene);
		foxes[3]->lookAt(ball_position, scene);

	for (auto& [id, fox] : foxes) {
		fox->animate((float)absolute_time, scene);
	}

	std::vector<int> to_delete ;
	for (auto& [id, packet] : packets) {
		bool alive = packet->animate((float)absolute_time,particles) ;
		if(!alive){
			to_delete.push_back(id) ;
		}
	}

	for(int k=0;k<to_delete.size();k++){
		packets.erase(to_delete[k]) ;
	}

	float anim_rate = 0.75f ;

	
	
}




// Called when switching out of this state after the last time run is called
void BallThrowApp::exit(std::shared_ptr<MachineState> to) {
	ScenePlugin* scene = getTool<ScenePlugin>();
	scene->deleteInstance(fox_id_1);
}


void BallThrowApp::createDefaultSequence(float start_time, float hold_time, float air_time){

	// left fox holds it at start
	foxes[0]->queue(start_time - FoxSequence::CATCH_TIME * foxes[0]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time , foxes[0]->getHeld());
	// left fox throws
	foxes[0]->queue(start_time+hold_time, FoxSequence::THROW);
	balls[0]->queue(start_time + hold_time, foxes[0]->getHeld());
	balls[0]->queue(start_time + hold_time + FoxSequence::THROW_WIND_TIME * foxes[0]->throw_speed, foxes[0]->getWind());
	balls[0]->queue(start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed, foxes[0]->getRelease());

	// ball goes left to right
	float t1 = start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed;
	glm::vec3 p1 = foxes[0]->getRelease() ;
	float t2 = start_time + hold_time + air_time ;
	glm::vec3 p2 = foxes[1]->getHeld();
	balls[0]->queueParabola(t1,p1,t2,p2,ball_peak,200) ;

	//right fox catches
	foxes[1]->queue(start_time +hold_time+air_time - FoxSequence::CATCH_TIME * foxes[1]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time + hold_time + air_time, foxes[1]->getHeld());

	//right fox throws
	foxes[1]->queue(start_time + air_time + hold_time*2.0f, FoxSequence::THROW);
	balls[0]->queue(start_time + air_time + hold_time * 2.0f, foxes[1]->getHeld());
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + FoxSequence::THROW_WIND_TIME * foxes[1]->throw_speed, foxes[1]->getWind());
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed, foxes[1]->getRelease());

	//ball goes right to left
	t1 = start_time + air_time + hold_time * 2.0f + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed;
	p1 = foxes[1]->getRelease();
	t2 = start_time + air_time * 2.0f + hold_time * 2.0f ;
	p2 = foxes[0]->getHeld();
	balls[0]->queueParabola(t1, p1, t2, p2, ball_peak, 200);

	//left fox catches
	foxes[0]->queue(start_time + air_time*2.0f + hold_time * 2.0f - FoxSequence::CATCH_TIME * foxes[0]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time + air_time * 2.0f + hold_time * 2.0f, foxes[0]->getHeld());

	// left fox holds it at end
}

void BallThrowApp::createServerStateSequence(float start_time, float hold_time, float air_time, float ping){

	
	// left fox holds it at start
	foxes[0]->queue(start_time - FoxSequence::CATCH_TIME * foxes[0]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time, foxes[0]->getHeld());

	foxes[2]->queue(start_time - FoxSequence::CATCH_TIME * foxes[2]->catch_speed, FoxSequence::CATCH);
	balls[1]->queue(start_time, foxes[2]->getHeld());

	// left fox throws for server
	foxes[0]->queue(start_time + hold_time, FoxSequence::THROW);
	balls[0]->queue(start_time + hold_time, foxes[0]->getHeld());
	balls[0]->queue(start_time + hold_time + FoxSequence::THROW_WIND_TIME * foxes[0]->throw_speed, foxes[0]->getWind());
	balls[0]->queue(start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed, foxes[0]->getRelease());

	// packet sent about state
	packets[(int)packets.size()] = std::make_shared<ParticleSequence>(ParticleSequence(particles, glm::vec4(0,0,1,1),
		glm::vec3(4,0,0), start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed, 
		glm::vec3(-4, 0, 0), start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed + ping*0.5f
	)) ;


	// left fox throws for client
	foxes[2]->queue(start_time + hold_time + ping*0.5f, FoxSequence::THROW);
	balls[1]->queue(start_time + hold_time + ping * 0.5f, foxes[2]->getHeld());
	balls[1]->queue(start_time + hold_time + ping * 0.5f + FoxSequence::THROW_WIND_TIME * foxes[2]->throw_speed, foxes[2]->getWind());
	balls[1]->queue(start_time + hold_time + ping * 0.5f + FoxSequence::THROW_RELEASE_TIME / foxes[2]->throw_speed, foxes[2]->getRelease());


	// ball goes left to right for server
	float t1 = start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed;
	glm::vec3 p1 = foxes[0]->getRelease();
	float t2 = start_time + hold_time + air_time;
	glm::vec3 p2 = foxes[1]->getHeld();
	balls[0]->queueParabola(t1, p1, t2, p2, ball_peak, 2000);

	// ball goes left to right for client
	t1 = start_time + ping*0.5f + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[2]->throw_speed;
	p1 = foxes[2]->getRelease();
	t2 = start_time + hold_time + air_time + ping*0.5f;
	p2 = foxes[3]->getHeld();
	balls[1]->queueParabola(t1, p1, t2, p2, ball_peak, 2000);


	//right fox catches for server
	foxes[1]->queue(start_time + hold_time + air_time - FoxSequence::CATCH_TIME * foxes[1]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time + hold_time + air_time, foxes[1]->getHeld());

	//right fox catches for client
	foxes[3]->queue(start_time + hold_time + air_time + ping*0.5f - FoxSequence::CATCH_TIME * foxes[3]->catch_speed, FoxSequence::CATCH);
	balls[1]->queue(start_time + hold_time + air_time + ping*0.5f, foxes[3]->getHeld());

	// packet goes from client to server for action
	packets[(int)packets.size()] = std::make_shared<ParticleSequence>(ParticleSequence(particles, glm::vec4(0, 0, 1, 1),
		glm::vec3(-4, 0, 0), start_time + air_time + hold_time * 2.0f + ping*0.5f,
		glm::vec3(4, 0, 0), start_time + air_time + hold_time * 2.0f + ping
	));

	//right fox throws for server
	foxes[1]->queue(start_time + air_time + hold_time * 2.0f + ping , FoxSequence::THROW);
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + ping , foxes[1]->getHeld());
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + ping  + FoxSequence::THROW_WIND_TIME * foxes[1]->throw_speed, foxes[1]->getWind());
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + ping  + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed, foxes[1]->getRelease());


	// packet from server to client for result
	packets[(int)packets.size()] = std::make_shared<ParticleSequence>(ParticleSequence(particles, glm::vec4(0, 0, 1, 1),
		glm::vec3(4, 0, 0), start_time + air_time + hold_time * 2.0f + ping ,
		glm::vec3(-4, 0, 0), start_time + air_time + hold_time * 2.0f + ping * 1.5f
	));
	
	//right fox throws for client
	foxes[3]->queue(start_time + air_time + hold_time * 2.0f + ping*0.5f, FoxSequence::THROW); //when they press the button
	balls[1]->queue(start_time + air_time + hold_time * 2.0f + ping * 1.5f, foxes[3]->getHeld()); // ball doesnt move until they get result
	balls[1]->queue(start_time + air_time + hold_time * 2.0f + ping * 1.5f + FoxSequence::THROW_WIND_TIME * foxes[3]->throw_speed, foxes[3]->getWind());
	balls[1]->queue(start_time + air_time + hold_time * 2.0f + ping * 1.5f + FoxSequence::THROW_RELEASE_TIME / foxes[3]->throw_speed, foxes[3]->getRelease());

	//ball goes right to left for server 
	t1 = start_time + air_time + hold_time * 2.0f + ping+ FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed;
	p1 = foxes[1]->getRelease();
	t2 = start_time + air_time * 2.0f + hold_time * 2.0f + ping;
	p2 = foxes[0]->getHeld();
	balls[0]->queueParabola(t1, p1, t2, p2, ball_peak, 2000);

	//ball goes right to left for client
	t1 = start_time + air_time + hold_time * 2.0f + ping*1.5f + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed;
	p1 = foxes[3]->getRelease();
	t2 = start_time + air_time * 2.0f + hold_time * 2.0f + ping*1.5f;
	p2 = foxes[2]->getHeld();
	balls[1]->queueParabola(t1, p1, t2, p2, ball_peak, 2000);

	//left fox catches for server
	foxes[0]->queue(start_time + air_time * 2.0f + hold_time * 2.0f + ping - FoxSequence::CATCH_TIME * foxes[0]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time + air_time * 2.0f + hold_time * 2.0f + ping, foxes[0]->getHeld());

	//left fox catches for client
	foxes[2]->queue(start_time + air_time * 2.0f + hold_time * 2.0f + ping*1.5f - FoxSequence::CATCH_TIME * foxes[2]->catch_speed, FoxSequence::CATCH);
	balls[1]->queue(start_time + air_time * 2.0f + hold_time * 2.0f + ping*1.5f, foxes[2]->getHeld());

	// left fox holds it at end


}


void BallThrowApp::createOwnerSequence(float start_time, float hold_time, float air_time, float ping) {


	// left fox holds it at start
	foxes[0]->queue(start_time - FoxSequence::CATCH_TIME * foxes[0]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time, foxes[0]->getHeld());

	foxes[2]->queue(start_time - FoxSequence::CATCH_TIME * foxes[2]->catch_speed, FoxSequence::CATCH);
	balls[1]->queue(start_time, foxes[2]->getHeld());

	// left fox throws for server
	foxes[0]->queue(start_time + hold_time, FoxSequence::THROW);
	balls[0]->queue(start_time + hold_time, foxes[0]->getHeld());
	balls[0]->queue(start_time + hold_time + FoxSequence::THROW_WIND_TIME * foxes[0]->throw_speed, foxes[0]->getWind());
	balls[0]->queue(start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed, foxes[0]->getRelease());

	// packet sent about state
	packets[(int)packets.size()] = std::make_shared<ParticleSequence>(ParticleSequence(particles, glm::vec4(0, 0, 1, 1),
		glm::vec3(4, 0, 0), start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed,
		glm::vec3(-4, 0, 0), start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed + ping * 0.5f
	));


	// left fox throws for client
	foxes[2]->queue(start_time + hold_time + ping * 0.5f, FoxSequence::THROW);
	balls[1]->queue(start_time + hold_time + ping * 0.5f, foxes[2]->getHeld());
	balls[1]->queue(start_time + hold_time + ping * 0.5f + FoxSequence::THROW_WIND_TIME * foxes[2]->throw_speed, foxes[2]->getWind());
	balls[1]->queue(start_time + hold_time + ping * 0.5f + FoxSequence::THROW_RELEASE_TIME / foxes[2]->throw_speed, foxes[2]->getRelease());


	// ball goes left to right for server
	float t1 = start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed;
	glm::vec3 p1 = foxes[0]->getRelease();
	float t2 = start_time + hold_time + air_time;
	glm::vec3 p2 = foxes[1]->getHeld();
	balls[0]->queueParabola(t1, p1, t2, p2, ball_peak, 2000);

	// ball goes left to right for client
	t1 = start_time + ping * 0.5f + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[2]->throw_speed;
	p1 = foxes[2]->getRelease();
	t2 = start_time + hold_time + air_time + ping * 0.5f;
	p2 = foxes[3]->getHeld();
	balls[1]->queueParabola(t1, p1, t2, p2, ball_peak, 2000);


	//right fox catches for server
	foxes[1]->queue(start_time + hold_time + air_time - FoxSequence::CATCH_TIME * foxes[1]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time + hold_time + air_time, foxes[1]->getHeld());

	//right fox catches for client
	foxes[3]->queue(start_time + hold_time + air_time + ping * 0.5f - FoxSequence::CATCH_TIME * foxes[3]->catch_speed, FoxSequence::CATCH);
	balls[1]->queue(start_time + hold_time + air_time + ping * 0.5f, foxes[3]->getHeld());

	// packet goes from client to server for action
	packets[(int)packets.size()] = std::make_shared<ParticleSequence>(ParticleSequence(particles, glm::vec4(0, 0, 1, 1),
		glm::vec3(-4, 0, 0), start_time + air_time + hold_time * 2.0f + ping * 0.5f,
		glm::vec3(4, 0, 0), start_time + air_time + hold_time * 2.0f + ping
	));

	//right fox throws for server
	foxes[1]->queue(start_time + air_time + hold_time * 2.0f + ping, FoxSequence::THROW);
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + ping, foxes[1]->getHeld());
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + ping + FoxSequence::THROW_WIND_TIME * foxes[1]->throw_speed, foxes[1]->getWind());
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + ping + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed, foxes[1]->getRelease());



	//right fox throws for client
	foxes[3]->queue(start_time + air_time + hold_time * 2.0f + ping * 0.5f, FoxSequence::THROW); //when they press the button
	balls[1]->queue(start_time + air_time + hold_time * 2.0f + ping * 0.5f, foxes[3]->getHeld()); // ball doesnt move until they get result
	balls[1]->queue(start_time + air_time + hold_time * 2.0f + ping * 0.5f + FoxSequence::THROW_WIND_TIME * foxes[3]->throw_speed, foxes[3]->getWind());
	balls[1]->queue(start_time + air_time + hold_time * 2.0f + ping * 0.5f + FoxSequence::THROW_RELEASE_TIME / foxes[3]->throw_speed, foxes[3]->getRelease());

	//ball goes right to left for server 
	t1 = start_time + air_time + hold_time * 2.0f + ping + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed;
	p1 = foxes[1]->getRelease();
	t2 = start_time + air_time * 2.0f + hold_time * 2.0f + ping;
	p2 = foxes[0]->getHeld();
	balls[0]->queueParabola(t1, p1, t2, p2, ball_peak, 2000);

	//ball goes right to left for client
	t1 = start_time + air_time + hold_time * 2.0f + ping * 0.5f + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed;
	p1 = foxes[3]->getRelease();
	t2 = start_time + air_time * 2.0f + hold_time * 2.0f + ping * 0.5f;
	p2 = foxes[2]->getHeld();
	balls[1]->queueParabola(t1, p1, t2, p2, ball_peak, 2000);

	//left fox catches for server
	foxes[0]->queue(start_time + air_time * 2.0f + hold_time * 2.0f + ping - FoxSequence::CATCH_TIME * foxes[0]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time + air_time * 2.0f + hold_time * 2.0f + ping, foxes[0]->getHeld());

	//left fox catches for client
	foxes[2]->queue(start_time + air_time * 2.0f + hold_time * 2.0f + ping * 0.5f - FoxSequence::CATCH_TIME * foxes[2]->catch_speed, FoxSequence::CATCH);
	balls[1]->queue(start_time + air_time * 2.0f + hold_time * 2.0f + ping * 0.5f, foxes[2]->getHeld());

	// left fox holds it at end


}



void BallThrowApp::createRollbackSequence(float start_time, float hold_time, float air_time, float ping) {


	// left fox holds it at start
	foxes[0]->queue(start_time - FoxSequence::CATCH_TIME * foxes[0]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time, foxes[0]->getHeld());

	foxes[2]->queue(start_time - FoxSequence::CATCH_TIME * foxes[2]->catch_speed, FoxSequence::CATCH);
	balls[1]->queue(start_time, foxes[2]->getHeld());

	// left fox throws for server
	foxes[0]->queue(start_time + hold_time, FoxSequence::THROW);
	balls[0]->queue(start_time + hold_time, foxes[0]->getHeld());
	balls[0]->queue(start_time + hold_time + FoxSequence::THROW_WIND_TIME * foxes[0]->throw_speed, foxes[0]->getWind());
	balls[0]->queue(start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed, foxes[0]->getRelease());

	// packet sent about state
	packets[(int)packets.size()] = std::make_shared<ParticleSequence>(ParticleSequence(particles, glm::vec4(0, 0, 1, 1),
		glm::vec3(4, 0, 0), start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed,
		glm::vec3(-4, 0, 0), start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed + ping * 0.5f
	));


	// left fox throws for client
	foxes[2]->queue(start_time + hold_time + ping * 0.5f, FoxSequence::THROW);
	balls[1]->queue(start_time + hold_time + ping * 0.5f, foxes[2]->getHeld());
	balls[1]->queue(start_time + hold_time + ping * 0.5f + FoxSequence::THROW_WIND_TIME * foxes[2]->throw_speed, foxes[2]->getWind());
	balls[1]->queue(start_time + hold_time + ping * 0.5f + FoxSequence::THROW_RELEASE_TIME / foxes[2]->throw_speed, foxes[2]->getRelease());


	// ball goes left to right for server
	float t1 = start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed;
	glm::vec3 p1 = foxes[0]->getRelease();
	float t2 = start_time + hold_time + air_time;
	glm::vec3 p2 = foxes[1]->getHeld();
	balls[0]->queueParabola(t1, p1, t2, p2, ball_peak, 2000);

	// ball goes left to right for client
	t1 = start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[2]->throw_speed;
	p1 = foxes[2]->getRelease();
	t2 = start_time + hold_time + air_time ;
	p2 = foxes[3]->getHeld();
	balls[1]->queueRollbackParabola(t1, p1, t2, p2, ball_peak, 2000, t1+ping*0.5f);


	//right fox catches for server
	foxes[1]->queue(start_time + hold_time + air_time - FoxSequence::CATCH_TIME * foxes[1]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time + hold_time + air_time, foxes[1]->getHeld());

	//right fox catches for client
	foxes[3]->queue(start_time + hold_time + air_time - FoxSequence::CATCH_TIME * foxes[3]->catch_speed, FoxSequence::CATCH);
	balls[1]->queue(start_time + hold_time + air_time , foxes[3]->getHeld());

	// packet goes from client to server for action
	packets[(int)packets.size()] = std::make_shared<ParticleSequence>(ParticleSequence(particles, glm::vec4(0, 0, 1, 1),
		glm::vec3(-4, 0, 0), start_time + air_time + hold_time * 2.0f ,
		glm::vec3(4, 0, 0), start_time + air_time + hold_time * 2.0f + ping*0.5f 
	));

	//right fox throws for server
	foxes[1]->queue(start_time + air_time + hold_time * 2.0f + ping * 0.5f, FoxSequence::THROW);
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + ping * 0.5f, foxes[1]->getHeld());
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + ping * 0.5f + FoxSequence::THROW_WIND_TIME * foxes[1]->throw_speed, foxes[1]->getWind());
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + ping * 0.5f + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed, foxes[1]->getRelease());



	//right fox throws for client
	foxes[3]->queue(start_time + air_time + hold_time * 2.0f , FoxSequence::THROW); //when they press the button
	balls[1]->queue(start_time + air_time + hold_time * 2.0f , foxes[3]->getHeld()); // ball doesnt move until they get result
	balls[1]->queue(start_time + air_time + hold_time * 2.0f + FoxSequence::THROW_WIND_TIME * foxes[3]->throw_speed, foxes[3]->getWind());
	balls[1]->queue(start_time + air_time + hold_time * 2.0f + FoxSequence::THROW_RELEASE_TIME / foxes[3]->throw_speed, foxes[3]->getRelease());

	//ball goes right to left for server 
	t1 = start_time + air_time + hold_time * 2.0f  + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed;
	p1 = foxes[1]->getRelease();
	t2 = start_time + air_time * 2.0f + hold_time * 2.0f ;
	p2 = foxes[0]->getHeld();
	balls[0]->queueRollbackParabola(t1, p1, t2, p2, ball_peak, 2000, t1+ping*0.5f);

	//ball goes right to left for client
	t1 = start_time + air_time + hold_time * 2.0f + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed;
	p1 = foxes[3]->getRelease();
	t2 = start_time + air_time * 2.0f + hold_time * 2.0f ;
	p2 = foxes[2]->getHeld();
	balls[1]->queueParabola(t1, p1, t2, p2, ball_peak, 2000);

	//left fox catches for server
	foxes[0]->queue(start_time + air_time * 2.0f + hold_time * 2.0f - FoxSequence::CATCH_TIME * foxes[0]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time + air_time * 2.0f + hold_time * 2.0f , foxes[0]->getHeld());

	//left fox catches for client
	foxes[2]->queue(start_time + air_time * 2.0f + hold_time * 2.0f - FoxSequence::CATCH_TIME * foxes[2]->catch_speed, FoxSequence::CATCH);
	balls[1]->queue(start_time + air_time * 2.0f + hold_time * 2.0f , foxes[2]->getHeld());

	// left fox holds it at end


}



void BallThrowApp::createRollbackInterpolationSequence(float start_time, float hold_time, float air_time, float ping, float interp) {


	// left fox holds it at start
	foxes[0]->queue(start_time - FoxSequence::CATCH_TIME * foxes[0]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time, foxes[0]->getHeld());

	foxes[2]->queue(start_time - FoxSequence::CATCH_TIME * foxes[2]->catch_speed, FoxSequence::CATCH);
	balls[1]->queue(start_time, foxes[2]->getHeld());

	// left fox throws for server
	foxes[0]->queue(start_time + hold_time, FoxSequence::THROW);
	balls[0]->queue(start_time + hold_time, foxes[0]->getHeld());
	balls[0]->queue(start_time + hold_time + FoxSequence::THROW_WIND_TIME * foxes[0]->throw_speed, foxes[0]->getWind());
	balls[0]->queue(start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed, foxes[0]->getRelease());

	// packet sent about state
	packets[(int)packets.size()] = std::make_shared<ParticleSequence>(ParticleSequence(particles, glm::vec4(0, 0, 1, 1),
		glm::vec3(4, 0, 0), start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed,
		glm::vec3(-4, 0, 0), start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed + ping * 0.5f
	));


	// left fox throws for client
	foxes[2]->queue(start_time + hold_time + ping * 0.5f, FoxSequence::THROW);
	balls[1]->queue(start_time + hold_time + ping * 0.5f, foxes[2]->getHeld());
	balls[1]->queue(start_time + hold_time + ping * 0.5f + FoxSequence::THROW_WIND_TIME * foxes[2]->throw_speed, foxes[2]->getWind());
	balls[1]->queue(start_time + hold_time + ping * 0.5f + FoxSequence::THROW_RELEASE_TIME / foxes[2]->throw_speed, foxes[2]->getRelease());


	// ball goes left to right for server
	float t1 = start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed;
	glm::vec3 p1 = foxes[0]->getRelease();
	float t2 = start_time + hold_time + air_time;
	glm::vec3 p2 = foxes[1]->getHeld();
	balls[0]->queueParabola(t1, p1, t2, p2, ball_peak, 2000);

	// ball goes left to right for client
	t1 = start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[2]->throw_speed;
	p1 = foxes[2]->getRelease();
	t2 = start_time + hold_time + air_time;
	p2 = foxes[3]->getHeld();
	balls[1]->queueRollbackInterpParabola(t1, p1, t2, p2, ball_peak, 2000, t1 + ping * 0.5f, interp);


	//right fox catches for server
	foxes[1]->queue(start_time + hold_time + air_time - FoxSequence::CATCH_TIME * foxes[1]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time + hold_time + air_time, foxes[1]->getHeld());

	//right fox catches for client
	foxes[3]->queue(start_time + hold_time + air_time - FoxSequence::CATCH_TIME * foxes[3]->catch_speed, FoxSequence::CATCH);
	balls[1]->queue(start_time + hold_time + air_time, foxes[3]->getHeld());

	// packet goes from client to server for action
	packets[(int)packets.size()] = std::make_shared<ParticleSequence>(ParticleSequence(particles, glm::vec4(0, 0, 1, 1),
		glm::vec3(-4, 0, 0), start_time + air_time + hold_time * 2.0f,
		glm::vec3(4, 0, 0), start_time + air_time + hold_time * 2.0f + ping * 0.5f
	));

	//right fox throws for server
	foxes[1]->queue(start_time + air_time + hold_time * 2.0f + ping * 0.5f, FoxSequence::THROW);
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + ping * 0.5f, foxes[1]->getHeld());
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + ping * 0.5f + FoxSequence::THROW_WIND_TIME * foxes[1]->throw_speed, foxes[1]->getWind());
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + ping * 0.5f + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed, foxes[1]->getRelease());



	//right fox throws for client
	foxes[3]->queue(start_time + air_time + hold_time * 2.0f, FoxSequence::THROW); //when they press the button
	balls[1]->queue(start_time + air_time + hold_time * 2.0f, foxes[3]->getHeld()); // ball doesnt move until they get result
	balls[1]->queue(start_time + air_time + hold_time * 2.0f + FoxSequence::THROW_WIND_TIME * foxes[3]->throw_speed, foxes[3]->getWind());
	balls[1]->queue(start_time + air_time + hold_time * 2.0f + FoxSequence::THROW_RELEASE_TIME / foxes[3]->throw_speed, foxes[3]->getRelease());

	//ball goes right to left for server 
	t1 = start_time + air_time + hold_time * 2.0f + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed;
	p1 = foxes[1]->getRelease();
	t2 = start_time + air_time * 2.0f + hold_time * 2.0f;
	p2 = foxes[0]->getHeld();
	balls[0]->queueRollbackInterpParabola(t1, p1, t2, p2, ball_peak, 2000, t1 + ping * 0.5f, interp);

	//ball goes right to left for client
	t1 = start_time + air_time + hold_time * 2.0f + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed;
	p1 = foxes[3]->getRelease();
	t2 = start_time + air_time * 2.0f + hold_time * 2.0f;
	p2 = foxes[2]->getHeld();
	balls[1]->queueParabola(t1, p1, t2, p2, ball_peak, 2000);

	//left fox catches for server
	foxes[0]->queue(start_time + air_time * 2.0f + hold_time * 2.0f - FoxSequence::CATCH_TIME * foxes[0]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time + air_time * 2.0f + hold_time * 2.0f, foxes[0]->getHeld());

	//left fox catches for client
	foxes[2]->queue(start_time + air_time * 2.0f + hold_time * 2.0f - FoxSequence::CATCH_TIME * foxes[2]->catch_speed, FoxSequence::CATCH);
	balls[1]->queue(start_time + air_time * 2.0f + hold_time * 2.0f, foxes[2]->getHeld());

	// left fox holds it at end


}

void BallThrowApp::createTimeWarpSequence(float start_time, float hold_time, float air_time, float ping, float C) {


	// left fox holds it at start
	foxes[0]->queue(start_time - FoxSequence::CATCH_TIME * foxes[0]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time, foxes[0]->getHeld());

	foxes[2]->queue(start_time - FoxSequence::CATCH_TIME * foxes[2]->catch_speed, FoxSequence::CATCH);
	balls[1]->queue(start_time, foxes[2]->getHeld());

	// left fox throws for server
	foxes[0]->queue(start_time + hold_time, FoxSequence::THROW);
	balls[0]->queue(start_time + hold_time, foxes[0]->getHeld());
	balls[0]->queue(start_time + hold_time + FoxSequence::THROW_WIND_TIME * foxes[0]->throw_speed, foxes[0]->getWind());
	balls[0]->queue(start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed, foxes[0]->getRelease());

	// packet sent about state
	packets[(int)packets.size()] = std::make_shared<ParticleSequence>(ParticleSequence(particles, glm::vec4(0, 0, 1, 1),
		glm::vec3(4, 0, 0), start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed,
		glm::vec3(-4, 0, 0), start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed + ping * 0.5f
	));


	// left fox throws for client
	foxes[2]->queue(start_time + hold_time + 8.0f / C, FoxSequence::THROW);
	balls[1]->queue(start_time + hold_time + 8.0f / C, foxes[2]->getHeld());
	balls[1]->queue(start_time + hold_time + 8.0f / C + FoxSequence::THROW_WIND_TIME * foxes[2]->throw_speed, foxes[2]->getWind());
	balls[1]->queue(start_time + hold_time + 8.0f / C + FoxSequence::THROW_RELEASE_TIME / foxes[2]->throw_speed, foxes[2]->getRelease());


	// ball goes left to right for server
	float t1 = start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[0]->throw_speed;
	glm::vec3 p1 = foxes[0]->getRelease();
	float t2 = start_time + hold_time + air_time;
	glm::vec3 p2 = foxes[1]->getHeld();
	balls[0]->queueWarpParabola(t1, p1, t2, p2, ball_peak, 2000, C, 4.0f);

	// ball goes left to right for client
	t1 = start_time + hold_time + FoxSequence::THROW_RELEASE_TIME / foxes[2]->throw_speed;
	p1 = foxes[2]->getRelease();
	t2 = start_time + hold_time + air_time;
	p2 = foxes[3]->getHeld();
	balls[1]->queueWarpParabola(t1, p1, t2, p2, ball_peak, 2000, C, -4.0f);


	//right fox catches for server
	foxes[1]->queue(start_time + hold_time + air_time - FoxSequence::CATCH_TIME * foxes[1]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time + hold_time + air_time, foxes[1]->getHeld());

	//right fox catches for client
	foxes[3]->queue(start_time + hold_time + air_time - FoxSequence::CATCH_TIME * foxes[3]->catch_speed, FoxSequence::CATCH);
	balls[1]->queue(start_time + hold_time + air_time, foxes[3]->getHeld());

	// packet goes from client to server for action
	packets[(int)packets.size()] = std::make_shared<ParticleSequence>(ParticleSequence(particles, glm::vec4(0, 0, 1, 1),
		glm::vec3(-4, 0, 0), start_time + air_time + hold_time * 2.0f,
		glm::vec3(4, 0, 0), start_time + air_time + hold_time * 2.0f + ping * 0.5f
	));

	if(8.0f / C < ping*0.5f){
		printf("not enough war pto hide latency, appearance is wrong!\n");
	}

	//right fox throws for server
	foxes[1]->queue(start_time + air_time + hold_time * 2.0f + 8.0f/C, FoxSequence::THROW);
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + 8.0f / C, foxes[1]->getHeld());
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + 8.0f / C + FoxSequence::THROW_WIND_TIME * foxes[1]->throw_speed, foxes[1]->getWind());
	balls[0]->queue(start_time + air_time + hold_time * 2.0f + 8.0f / C + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed, foxes[1]->getRelease());



	//right fox throws for client
	foxes[3]->queue(start_time + air_time + hold_time * 2.0f, FoxSequence::THROW); //when they press the button
	balls[1]->queue(start_time + air_time + hold_time * 2.0f, foxes[3]->getHeld()); // ball doesnt move until they get result
	balls[1]->queue(start_time + air_time + hold_time * 2.0f + FoxSequence::THROW_WIND_TIME * foxes[3]->throw_speed, foxes[3]->getWind());
	balls[1]->queue(start_time + air_time + hold_time * 2.0f + FoxSequence::THROW_RELEASE_TIME / foxes[3]->throw_speed, foxes[3]->getRelease());

	//ball goes right to left for server 
	t1 = start_time + air_time + hold_time * 2.0f + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed;
	p1 = foxes[1]->getRelease();
	t2 = start_time + air_time * 2.0f + hold_time * 2.0f;
	p2 = foxes[0]->getHeld();
	balls[0]->queueWarpParabola(t1, p1, t2, p2, ball_peak, 2000, C, 4.0f);

	//ball goes right to left for client
	t1 = start_time + air_time + hold_time * 2.0f + FoxSequence::THROW_RELEASE_TIME / foxes[1]->throw_speed;
	p1 = foxes[3]->getRelease();
	t2 = start_time + air_time * 2.0f + hold_time * 2.0f;
	p2 = foxes[2]->getHeld();
	balls[1]->queueWarpParabola(t1, p1, t2, p2, ball_peak, 2000, C, -4.0f);

	//left fox catches for server
	foxes[0]->queue(start_time + air_time * 2.0f + hold_time * 2.0f - FoxSequence::CATCH_TIME * foxes[0]->catch_speed, FoxSequence::CATCH);
	balls[0]->queue(start_time + air_time * 2.0f + hold_time * 2.0f, foxes[0]->getHeld());

	//left fox catches for client
	foxes[2]->queue(start_time + air_time * 2.0f + hold_time * 2.0f - FoxSequence::CATCH_TIME * foxes[2]->catch_speed, FoxSequence::CATCH);
	balls[1]->queue(start_time + air_time * 2.0f + hold_time * 2.0f, foxes[2]->getHeld());

	// left fox holds it at end


}