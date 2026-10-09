#ifndef _NET_PHYSICS_APP_H_
#define _NET_PHYSICS_APP_H_ 1

#include "AsyncPlugin.h"
#include "MachineState.h"
#include "Registry.h"
#include "NetPhysics.h"
#include "PanelPlugin.h"

class NetPhysicsApp : public MachineState {

public:



	static inline const std::string state_name = "net_physics_state";
	static inline const std::string WORLD = "netphysics" ;

	NetPhysicsApp();

	//Called every frame while the state is active
	void run() override;

	// Called when switching into this state before the first time run is called
	void enter(std::shared_ptr<MachineState> from) override;

	// Called when switching out of this state after the last time run is called
	void exit(std::shared_ptr<MachineState> to) override;

	void updateCamera();


	void createViewTypes();

	void host();

	int debug_panel = -1;
	std::shared_ptr<PanelPlugin::Label> debug_label;
	int debug_frames = 0;
	float total_dt = 0;
	int debug_fps = 0;
	bool debug_panel_enabled = true;

	void updateDebugPanel();

private:

	int light_id = -1; // Scene light
	float mouse_depth = 11.0f;
	float mouse_size = 0.1f;
	int mouse_particle_id = -1; 
	std::shared_ptr<RayGrab> mouse_action = std::make_shared<RayGrab>() ;
	bool space_down = false;

	std::chrono::high_resolution_clock::time_point last_run_time;
	std::chrono::high_resolution_clock::time_point current_time;

	int port = 4225 ;
	std::string version = "np0.01" ;
	std::chrono::high_resolution_clock::time_point connect_time = now();
	int64_t cell_id = -1 ;


	glm::vec3 min = { -4,-4,-4 };
	glm::vec3 max = { 4,4,4 };
	float gravity = 4.0f;
	int millis_between_balls = 300;
	int max_balls = 150;

	// Camera control stuff
	glm::vec3 look_at = glm::vec3(0, -2, 0);
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
	float light_zoom = 30.0f;
	float light_fov = 0.7f;
	float light_theta = 0.4f;
	float light_thi = 1.5f;
	float mouse_wheel_y_previous = 0.0f;

	static inline const std::string BALL_MODEL = "./Narball/asset/BeachBall.glb";
	static inline const std::string JAR_MODEL = "./assets/TSP_Jar_01A.glb";
	static inline const std::string FOX_MODEL = "./assets/Fox2_base.glb";
	static inline const std::string BUNNY_MODEL = "./assets/cut_bunny.glb";
	static inline const std::string BUNNY_VISUAL_MODEL = "./assets/uncut_bunny.glb";
	static inline const std::string CHAIN_MODEL = "./assets/chain_link.glb";
	static inline const std::string CHAIN_MODEL_CUT = "./assets/chain_link_cut.glb";

	int box_type = -1;
	int ball_type = -1;
	int wall_type = -1;
	int rod_type = -1;
	int jar_type = -1;
	int bunny_type = -1;
	int chain_type = -1;
};
#endif // #ifndef _CONSTRAINT_TEST_APP_H_