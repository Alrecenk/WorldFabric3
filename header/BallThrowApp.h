#ifndef _BALL_THROW_APP_H_
#define _BALL_THROW_APP_H_ 1

#include "AsyncPlugin.h"
#include "MachineState.h"
#include "VulkanPlugin.h"
#include "ScenePlugin.h"
#include "ParticlePlugin.h"
#include "glm/glm.hpp"
#include "GLTF.h"
#include <stdio.h>
#include <cstdlib>

#include <string>
#include <map>




class BallThrowApp : public MachineState {

public:

	static inline float ball_scale = 0.3f ;

	class FoxSequence{
		
	public:
		//State a fox can be in
		static inline int IDLE = 1;
		static inline int CATCH = 2;
		static inline int HOLD = 3;
		static inline int THROW = 4 ;

		// local coordinates for various ball positions ;
		static inline glm::vec4 HELD_OFFSET = {0,0.65f,-0.08f, 1.0f}  ; 
		static inline glm::vec4 WIND_OFFSET = {0,0.885,-0.27, 1.0f} ;
		static inline glm::vec4 RELEASE_OFFSET = {0,0.925,-0.13, 1.0f};
		static inline int neck_bone = -1; 
		//time within animation that events occur
		static inline float THROW_WIND_TIME = 1.0f/12.0f ;
		static inline float THROW_RELEASE_TIME = 2.0f / 12.0f;
		static inline float CATCH_TIME  = 1.0f/3.0f ;

		int scene_id = -1;
		int tail_sway_anim = -1 ;
		int head_idle_anim = -1;
		int action_anim = -1 ;
		int state = IDLE;
		glm::quat base_rotation ;
		glm::quat target_rotation  ;
		glm::mat4 pose = glm::mat4(1.0f);
		float catch_speed = 1.0f;
		float throw_speed = 1.0f;
		float anim_start_time = -1 ;
		
		int next_action = 0 ;
		std::vector<std::pair<float, int>> actions ;

		FoxSequence(glm::mat4 pose, ScenePlugin* scene){
			this->pose = pose ;
			scene_id = scene->createInstance("fox", pose);
			tail_sway_anim = scene->animateInstance(scene_id, "tail_sway", true);
			head_idle_anim = scene->animateInstance(scene_id, "head_idle", true);
			base_rotation = scene->createPin(scene_id, "neck", neck_bone, glm::vec3(0, 0, 0), 0.0f, 1.0f); // create a rotation IK pin on the head
			target_rotation = base_rotation;
			scene->enableIK(scene_id, true);

		}
		

		void lookAt(glm::vec3& look_at_position, ScenePlugin* scene){
			// get local coordinates of fox instance
			glm::vec3 current = scene->getPinPosition(scene_id, "neck");
			glm::vec3 forward = pose * glm::vec4(0, 0, 1.0f, 0.0f); // a forward looking vector
			forward = glm::normalize(forward);
			glm::vec3 right = pose * glm::vec4(1.0f, 0, 0, 0.0f);
			right = normalize(right);
			glm::vec3 up = pose * glm::vec4(0.0, 1.0f, 0, 0.0f);
			up = normalize(up);

			// compute angle to look at target
			glm::vec3 lookat_vec = look_at_position - current;

			

			lookat_vec = glm::normalize(lookat_vec);

			//don't try too hard
			if (glm::dot(lookat_vec, forward) < 0.6f){
				lookat_vec = forward ;
			}

			glm::quat pose_forward_rot = quatLookAt(-forward, up) * base_rotation;
			glm::quat new_target_rot = quatLookAt(-lookat_vec, up) * base_rotation;
			float angle = glm::angle(pose_forward_rot * glm::inverse(new_target_rot));


			target_rotation = GLTF::slerp(target_rotation, new_target_rot, 0.05f); // general smoothing
			scene->setPinTarget(scene_id, "neck", target_rotation);

		}

		void queue(float t, int a){
			actions.push_back(std::make_pair(t, a)) ;
		}

		void animate(float time, ScenePlugin* scene){
			
			if(next_action < actions.size()){
				//printf("pending:%d %f >  %f\n", scene_id, actions[next_action].first, time);
				if(time > actions[next_action].first ){
					int act = actions[next_action].second ;
					if(act == CATCH && state != CATCH){
						action_anim = scene->animateInstance(scene_id, "ball_catch", false, 0, catch_speed);
						scene->setAnimationWeight(scene_id, action_anim, 50.0);
						state = CATCH ;
						anim_start_time = time ;
						//printf("catch:%d %f \n", scene_id, time) ;
					}else if(act == HOLD){
		

					}else if(act == THROW && state != THROW){
						scene->clearAnimation(scene_id, action_anim);
						action_anim = scene->animateInstance(scene_id, "ball_throw", false, 0, throw_speed);
						scene->setAnimationWeight(scene_id, action_anim, 50.0);
						state = THROW ;
						anim_start_time = time;
						//printf("throw:%d %f \n", scene_id, time);
					}else if(act == IDLE){

					}

					next_action++;
				}
				
			}

			if (state == THROW && scene->animationDone(scene_id, action_anim)) {
				float w = scene->getAnimationWeight(scene_id, action_anim) ;
				w *= 0.75f ;
				if(w < 0.01){
					scene->clearAnimation(scene_id, action_anim);
					state = IDLE;
				}else{
					scene->setAnimationWeight(scene_id, action_anim, w);
				}
			}

		}

		//returns ball position if fox is holding it
		glm::vec3 getBallPosition(float time){
			if(state == THROW){
				double wind_time = anim_start_time + THROW_WIND_TIME / throw_speed;
				double release_time = anim_start_time + THROW_RELEASE_TIME / throw_speed;
				if (time < wind_time) {
					float t = (float)((time - anim_start_time) / (wind_time - anim_start_time));
					glm::vec3 ball_pos = HELD_OFFSET * (1 - t) + WIND_OFFSET * t;
					return glm::vec3(pose * glm::vec4(ball_pos, 1.0f));

				}
				else if (time < release_time + 3.0) {
					float t = (float)((time - wind_time) / (release_time - wind_time));
					glm::vec3 ball_pos = WIND_OFFSET * (1 - t) + RELEASE_OFFSET * t;
					return glm::vec3(pose * glm::vec4(ball_pos, 1.0f));
				}
			}else if(state == HOLD || state == CATCH){
				return glm::vec3(pose * HELD_OFFSET) ;
			}else{
				return glm::vec3(0,0,0) ;
			}
		
		}


		glm::vec3 getHeld(){
			return glm::vec3(pose * HELD_OFFSET);
		}

		glm::vec3 getWind() {
			return glm::vec3(pose * WIND_OFFSET);
		}

		glm::vec3 getRelease() {
			return glm::vec3(pose * RELEASE_OFFSET);
		}
	
	};

	class BallSequence{
	public:
		std::vector<std::pair<float, glm::vec3>> position ;
		int scene_id =1 ;
		int index = 0 ;

		BallSequence(ScenePlugin* scene){
			glm::mat4 ball_pose = glm::mat4(1.0f);
			ball_pose = glm::scale(ball_pose, glm::vec3(ball_scale, ball_scale, ball_scale));
			scene_id = scene->createInstance("ball", ball_pose);
		}
		
		void queue(float t, glm::vec3 p) {
			/*if(position.size()>1 &&  t < position[position.size()-1].first - 0.001f){
				throw std::runtime_error("out of time ball move!");
			}*/

			position.push_back(std::make_pair(t, p));
		}

		glm::vec3 animate(float time, ScenePlugin* scene) {
			if (index + 1 < position.size()) {
				//step forward untoil indexis the klast index before currenttime
				while(time > position[index + 1].first && index + 2 < position.size()) {
					index++;
				}

				float t = (float)((time - position[index].first) / (position[index+1].first - position[index].first));
				glm::vec3 ball_pos = position[index].second * (1 - t) + position[index + 1].second * t;

				glm::mat4 ball_pose = glm::mat4(1.0);
				ball_pose = glm::translate(ball_pose, ball_pos);
				ball_pose = glm::scale(ball_pose, glm::vec3(ball_scale, ball_scale, ball_scale));
				scene->setPose(scene_id, ball_pose);
				
				return ball_pos ;
			}
			return glm::vec3(0,0,0) ;

		}


		void queueSpline(float time_1, glm::vec3 p1, glm::vec3 v1, float time_2, glm::vec3 p2, glm::vec3 v2, int steps){
			//printf("t1 %f, t2 %f\n", time_1, time_2) ;
			for(int k=0;k<steps;k++){
				float t = k / (steps-1.0f) ;
				float t2 = t*t ;
				float t3 = t*t2 ;
				glm::vec3 p  = p1 * (2.0f*t3 - 3.0f*t2 + 1.0f) + v1 * ( t3 - 2.0f*t2 + t) + p2 * (-2.0f*t3 + 3.0f*t2) + v2 * (t3 - t2) ;
				float time = time_1 * (1.0f-t) + time_2 * t ;
				//printf(" t= %f spline time: %f  p: %f,%f,%f\n", t, time, p.x, p.y, p.z) ;
				queue(time, p);
			}

		}

		void queueParabola(float time_1, glm::vec3 p1, float time_2, glm::vec3 p2, float height, int steps){

			for (int k = 0; k < steps; k++) {
				float t = k / (steps - 1.0f);

				float x = (1.0f - t) * p1.x + t * p2.x ;
				float z = (1.0f - t) * p1.z + t * p2.z;
				float y = (1.0f - t) * p1.y + t * p2.y;

				y += (4 * t - 4 * t*t)* height ;
				float time = time_1 * (1.0f - t) + time_2 * t;
				queue(time, glm::vec3(x,y,z));
			}

		}

		void queueWarpParabola(float time_1, glm::vec3 p1, float time_2, glm::vec3 p2, float height, int steps, float C, float vx) {

			for (int k = 0; k < steps; k++) {
				float t = k / (steps - 1.0f);

				float x = (1.0f - t) * p1.x + t * p2.x;
				float z = (1.0f - t) * p1.z + t * p2.z;
				float y = (1.0f - t) * p1.y + t * p2.y;

				y += (4 * t - 4 * t * t) * height;
				float time = time_1 * (1.0f - t) + time_2 * t;
				time += fabs(vx-x)/C ;
				queue(time, glm::vec3(x, y, z));
			}

		}

		// sits at p1 until packet_time then teleports into the the parabole
		void queueRollbackParabola(float time_1, glm::vec3 p1, float time_2, glm::vec3 p2, float height, int steps, float packet_time) {
			//printf("times %f, %f, %f \n", time_1, time_2, packet_time) ;
			//printf(" p1 %f, %f, %f \n", p1.x, p1.y, p1.z);
			//printf(" p2 %f, %f, %f \n", p2.x, p2.y, p2.z);
			for (int k = 0; k < steps; k++) {
				float t = k / (steps - 1.0f);

				float x = (1.0f - t) * p1.x + t * p2.x;
				float z = (1.0f - t) * p1.z + t * p2.z;
				float y = (1.0f - t) * p1.y + t * p2.y;

				y += (4 * t - 4 * t * t) * height;
				float time = time_1 * (1.0f - t) + time_2 * t;
				if(time < packet_time){
					if(time > position[position.size() - 1].first){
						queue(time, p1);
						//printf("a %f \n", time);
					}
				}else{
					queue(time, glm::vec3(x, y, z));
					//printf("b %f \n", time);
				}
			}

		}

		// sits at p1 until packet_time then teleports into the the parabole
		void queueRollbackInterpParabola(float time_1, glm::vec3 p1, float time_2, glm::vec3 p2, float height, int steps, float packet_time, float interp) {
			//printf("times %f, %f, %f \n", time_1, time_2, packet_time) ;
			//printf(" p1 %f, %f, %f \n", p1.x, p1.y, p1.z);
			//printf(" p2 %f, %f, %f \n", p2.x, p2.y, p2.z);
			for (int k = 0; k < steps; k++) {
				float t = k / (steps - 1.0f);

				float x = (1.0f - t) * p1.x + t * p2.x;
				float z = (1.0f - t) * p1.z + t * p2.z;
				float y = (1.0f - t) * p1.y + t * p2.y;

				y += (4 * t - 4 * t * t) * height;
				float time = time_1 * (1.0f - t) + time_2 * t;
				if (time < packet_time) {
					if (time > position[position.size() - 1].first) {
						queue(time, p1);
						//printf("a %f \n", time);
					}
				}
				else {
					if( time < packet_time+interp){
						float s = (time - packet_time)/interp ;
						queue(time,p1 * (1.0f-s) +  glm::vec3(x, y, z) * s);

					}else{
				
						queue(time, glm::vec3(x, y, z));
					}
					//printf("b %f \n", time);
				}
			}

		}


	};


	class ParticleSequence{
	public:
		float t1 ;
		float t2 ;
		glm::vec3 p1 ;
		glm::vec3 p2;
		int id = -1 ;
		static inline float particle_size = 0.2f ;

		ParticleSequence(ParticlePlugin* particles, glm::vec4 color, glm::vec3 p1, float t1, glm::vec3 p2, float t2){
			
			id = particles->createParticle(0) ;
			particles->setColor(id, color);
			//printf("particle made %d  t1  %f to t2 %f\n", id, t1, t2);
			this->t1 = t1 ;
			this->t2 = t2 ;
			this->p1 = p1 ;
			this->p2 = p2 ;
		}


		bool animate(float time, ParticlePlugin* particles){
			//printf("particle animating %d  %f < %f < %f\n", id, t1, time, t2);
			if(time > t2){
				particles->destroyParticle(id);
				//printf("particle deleted %d\n", id) ;
				return false;
			}else if(time > t1){
				float t = (time - t1)/ (t2-t1) ;
				glm::vec3 p = p1 * (1.0f-t) + p2 * t ;
				glm::mat4 pose = glm::mat4(1.0f);
				pose = glm::translate(pose, p);
				pose = glm::scale(pose, glm::vec3(particle_size));
				particles->setPose(id, pose);
				//printf("particle moved : %f,%f,%f\n", p.x, p.y,p.z) ;
			}else{

			}
			return true;
		}
	};


	static inline const std::string state_name = "ball_throw_state";

	//Loads models from the hard drive on construction
	BallThrowApp();

	void run() override;

	// Called when switching into this sate before the first time run is claled
	void enter(std::shared_ptr<MachineState> from) override;


	// Called when switching outof this state after the last time run is called
	void exit(std::shared_ptr<MachineState> to) override;




private:


	ParticlePlugin* particles ;
	std::chrono::high_resolution_clock::time_point last_run_time;
	std::chrono::high_resolution_clock::time_point current_time;

	int fox_id_1=-1;
	int fox_id_2=-1;
	int tail_sway_id_1 = -1;
	int tail_sway_id_2 = -1 ;
	int action_id_1 =-1;
	int action_id_2 = -1 ;
	int catch_state_1 = 0 ;

	int ball_id =-1;
	glm::quat base_rotation;
	glm::quat target_rot;


	std::vector<int> moving_lights;
	glm::vec3 ball_held = glm::vec3(4.08f,0.65f,0.0f) ; // position of ball hold for left_foxc mirrored for right
	glm::vec3 ball_throw_wind = glm::vec3(4.27f,0.685,0) ;
	float ball_throw_wind_time = 1.0f/12.0f ; // time after ball throw start where ball should be at ball_throw_wind
	float ball_release_time = 2.0f/12.0f; // time after ball_throw start where all shouldbe at ball_release
	glm::vec3 ball_release = glm::vec3(4.13,0.925f,0.0f) ;

	double absolute_time = 0 ;
	double anim_start_time = -1000;


	std::map<int, std::shared_ptr<FoxSequence>> foxes ;
	std::map<int, std::shared_ptr<BallSequence>> balls;
	std::map<int, std::shared_ptr<ParticleSequence>> packets ;
	float ball_peak = 1.5f ;

	void createDefaultSequence(float start_time, float hold_time, float air_time);
	

	void createServerStateSequence(float start_time, float hold_time, float air_time, float ping);

	void createOwnerSequence(float start_time, float hold_time, float air_time, float ping);


	void createRollbackSequence(float start_time, float hold_time, float air_time, float ping);

	void createRollbackInterpolationSequence(float start_time, float hold_time, float air_time, float ping, float interp);

	void createTimeWarpSequence(float start_time, float hold_time, float air_time, float ping, float C);
};
#endif // #ifndef _BALL_THROW_APP_H_