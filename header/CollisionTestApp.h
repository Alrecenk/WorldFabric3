#ifndef _COLLISION_TEST_APP_H_
#define _COLLISION_TEST_APP_H_ 1

#include "AsyncPlugin.h"
#include "MachineState.h"
#include "Registry.h"
#include "Utilities.h" // used for glm::vec3 hash
#include "Physics.h"

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

	std::map<std::string, std::shared_ptr<Physics::ConvexPolyhedron>> base_shape;
	std::vector<Physics::RigidBody> instances; // maps scene instance to transform of base shape
	std::vector<int> scene_ids ;
	double time = 0 ;

	


	std::vector<glm::vec4> colors = { {1,0,0,0.3}, {0,1,0,0.3},{0,0,1,0.3} };
	std::vector<glm::vec3> positions = { {0,0.6-0.1,-0.2}, {0,0.6,0},{0,0.6-0.1,0.2} };
	float particle_size = 0.005f ;
	int light_id = -1; // Scene light
	int mouse_particle_id = -1;
	std::chrono::high_resolution_clock::time_point last_run_time;
	std::chrono::high_resolution_clock::time_point current_time;

	glm::vec3 min = { -1.5,0,-1.5};
	glm::vec3 max = { 1.5,2.5,1.5 };
	int room_instance_id  = -1 ;

	std::vector<int> display_particles;
	std::vector<int> last_display_particles;

	// Csmera control stuff
	glm::vec3 look_at = glm::vec3(0, 0.6f, 0);
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
	float zoom = 2.5f;
	float light_zoom = 2.0f;
	float light_fov = 1.0f;
	float light_theta = 0.4f;
	float light_thi = 1.2f;
	float mouse_wheel_y_previous = 0.0f;

};

#endif // #ifndef _COLLISION_TEST_APP_H_