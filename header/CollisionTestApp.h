#ifndef _COLLISION_TEST_APP_H_
#define _COLLISION_TEST_APP_H_ 1

#include "AsyncPlugin.h"
#include "MachineState.h"
#include "Registry.h"

class CollisionTestApp : public MachineState {

public:


	static inline const std::string state_name = "collision_test_state";

	CollisionTestApp();

	//Called every frame while the state is active
	void run() override;

	// Called when switching into this state before the first time run is called
	void enter(std::shared_ptr<MachineState> from) override;

	// Called when switching out of this state after the last time run is called
	void exit(std::shared_ptr<MachineState> to) override;

	void updateCamera();


private:

	int light_id = -1; // Scene light
	int mouse_particle_id = -1;
	std::chrono::high_resolution_clock::time_point last_run_time;
	std::chrono::high_resolution_clock::time_point current_time;

	std::chrono::high_resolution_clock::time_point last_ball_time = now();



	glm::vec3 min = { -3,-4,-3 };
	glm::vec3 max = { 3,4,3 };
	float gravity = 4.0f;
	int millis_between_balls = 600;
	int max_balls = 250;

	// Csmera control stuff
	glm::vec3 look_at = glm::vec3(0, -3, 0);
	glm::vec3 light_look_at = glm::vec3(0, 0, 0);
	float fov = 1.0f;
	float camera_theta = 0.5f;
	float camera_thi = 0.8f;
	bool mouse_down_left = false;
	glm::vec2 mouse_down_position_left;
	bool mouse_down_right = false;
	glm::vec2 mouse_down_position_right;
	float camera_down_theta = 0.0f;
	float camera_down_thi = 0.0f;
	float camera_x_speed = 0.002f;
	float camera_y_speed = 0.002f;
	float zoom = 11.0f;
	float light_zoom = 20.0f;
	float light_fov = 1.0f;
	float light_theta = 0.4f;
	float light_thi = 1.2f;
	float mouse_wheel_y_previous = 0.0f;
};
#endif // #ifndef _COLLISION_TEST_APP_H_