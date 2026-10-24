#ifndef _NET_PHYSICS_APP2_H_
#define _NET_PHYSICS_APP2_H_ 1

#include "AsyncPlugin.h"
#include "MachineState.h"
#include "Registry.h"
#include "NetPhysics2.h"
#include "PanelPlugin.h"
#include "SteamworksPlugin.h"

class NetPhysicsApp2 : public MachineState, public SteamworksPlugin::SteamEventReceiver {

public:



	static inline const std::string state_name = "net_physics_state2";
	static inline const std::string WORLD = "netphysics";

	NetPhysicsApp2();

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

	void onSteamGameExternalJoin(std::shared_ptr<SteamworksPlugin::SteamSocket> socket, const SteamworksPlugin::SteamServerInfo& server_info) override;

	void startSteamHosting();

private:

	int light_id = -1; // Scene light
	float mouse_depth = 11.0f;
	float mouse_size = 0.1f;
	int mouse_particle_id = -1;
	std::shared_ptr<RayGrab> mouse_action = std::make_shared<RayGrab>();
	std::shared_ptr<PosedGrab> posed_action_left = std::make_shared<PosedGrab>(0.025f);
	std::shared_ptr<PosedGrab> posed_action_right = std::make_shared<PosedGrab>(0.025f);
	int cursor_instance_left = -1;
	int cursor_instance_right = -1;
	glm::mat4 cursor_adjust ;
	float cursor_angle_1 = 3.1415926f;
	float cursor_angle_2 = (float)(3.1415936*0.25) ;

	bool space_down = false;

	std::chrono::high_resolution_clock::time_point last_run_time;
	std::chrono::high_resolution_clock::time_point current_time;

	int port = 4225;
	std::string version = "np0.01";
	std::chrono::high_resolution_clock::time_point connect_time = now();
	int64_t root_id = -1;


	glm::vec3 min = { -0.75,0.5,-0.75 };
	glm::vec3 max = { 0.75,2.5,0.75 };
	float cell_size = 2.0f ;
	float wall_size = 2.0f;


	// Camera control stuff
	glm::vec3 look_at = glm::vec3(0, 0.5, 0);
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
	float zoom = 1.0f;
	float light_zoom = 3.0f;
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
	static inline const std::string CURSOR_MODEL = "./assets/cursor.glb";

	int box_type = -1;
	int ball_type = -1;
	int wall_type = -1;
	int rod_type = -1;
	int jar_type = -1;
	int bunny_type = -1;
	int chain_type = -1;
};
#endif // #ifndef _NETPHYSICS_APP2_H_