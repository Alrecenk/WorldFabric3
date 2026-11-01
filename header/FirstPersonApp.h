#ifndef _FIRST_PERSON_APP_H_
#define _FIRST_PERSON_APP_H_ 1

#include "AsyncPlugin.h"
#include "MachineState.h"
#include "Registry.h"
#include "NetPhysics2.h"
#include "PanelPlugin.h"
#include "SteamworksPlugin.h"


class PlayerCharacter : public WorldObject {
public:
	int64_t root_id = -1;
	int64_t body_id = -1;
	glm::vec3 velocity = glm::vec3(0,0,0);
	glm::vec3 control = glm::vec3(0, 0, 0);
	bool grounded = false;
	double last_control_time ;
	float jump_time = 0.0f;

	static inline float max_speed = 10.0f ;
	static inline float max_control_speed = 5.0f ; // Controls can't push faster than this
	static inline float player_acceleration = 16.0f;
	static inline float gravity = -8.0f; // incorporates drag
	static inline float drag = 4.0f;
	static inline double time_out_time = 3.0 ;
	static inline double interframe_time = 0.2 ;
	static inline float total_jump_time = 0.2f ;
	static inline float jump_force  = 7.0f ;
	static inline float initial_jump_velocity = 3.0f ;
	


	PlayerCharacter(){};

	PlayerCharacter(int64_t root, const glm::vec3& pos) ;

	//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
		// Just change the template parameter to match your class
	int getTypeId(Registry* r) const override {
		return r->getIdForType<PlayerCharacter>();
	}

	std::shared_ptr<WorldObject> deepCopy() const override {
		return std::make_shared<PlayerCharacter>(*this);
	}

	//Functions used on observables or on read objects need to be const
	void print() const override {
		printf("FirstPersonPlayerCharacter");
	}


	void update();

	void setPosition(const glm::vec3& new_position);

	void setControl(const glm::vec3& new_control, const bool& jumping);

};

auto static getStructure(PlayerCharacter& o) {
	return std::tie(o.position,o.root_id, o.body_id, o.velocity, o.control, o.last_control_time, o.grounded, o.jump_time);
}

class FirstPersonApp : public MachineState, public SteamworksPlugin::SteamEventReceiver {

public:



	static inline const std::string state_name = "first_person_state";
	static inline const std::string WORLD = "netphysics";

	FirstPersonApp();

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


	static inline const std::string BALL_MODEL = "./Narball/asset/BeachBall.glb";
	static inline const std::string JAR_MODEL = "./assets/TSP_Jar_01A.glb";
	static inline const std::string RUIN_MODEL = "./assets/TowerRuinCollision.glb";
	static inline const std::string RUIN_VISUAL_MODEL = "./assets/TowerRuinVisual.glb";
	static inline const std::string BUNNY_MODEL = "./assets/cut_bunny.glb";
	static inline const std::string BUNNY_VISUAL_MODEL = "./assets/uncut_bunny.glb";
	static inline const std::string CURSOR_MODEL = "./assets/cursor.glb";

	static inline int box_type = -1;
	static inline int ball_type = -1;
	static inline int wall_type = -1;
	static inline int rod_type = -1;
	static inline int jar_type = -1;
	static inline int bunny_type = -1;
	static inline int ruin_type = -1;
	static inline int player_type  = -1 ;

private:

	int64_t my_player_id = -1 ;
	glm::vec3 last_control = glm::vec3(0,0,0);
	glm::vec3 look_direction = glm::vec3(0,0,1) ;
	std::chrono::high_resolution_clock::time_point last_input_time = now();

	int light_id = -1; // Scene light
	float mouse_depth = 11.0f;
	float mouse_size = 0.1f;
	int mouse_particle_id = -1;
	std::shared_ptr<RayGrab> mouse_action = std::make_shared<RayGrab>();
	std::shared_ptr<PosedGrab> posed_action_left = std::make_shared<PosedGrab>(0.025f);
	std::shared_ptr<PosedGrab> posed_action_right = std::make_shared<PosedGrab>(0.025f);
	int cursor_instance_left = -1;
	int cursor_instance_right = -1;
	glm::mat4 cursor_adjust;
	float cursor_angle_1 = 3.1415926f;
	float cursor_angle_2 = (float)(3.1415936 * 0.25);

	bool space_down = false;

	std::chrono::high_resolution_clock::time_point last_run_time;
	std::chrono::high_resolution_clock::time_point current_time;

	int port = 4225;
	std::string version = "np0.01";
	std::chrono::high_resolution_clock::time_point connect_time = now();
	int64_t root_id = -1;


	glm::vec3 min = { -0.75,0.5,-0.75 };
	glm::vec3 max = { 0.75,2.5,0.75 };
	float cell_size = 2.0f;
	float wall_size = 2.0f;

	bool mouse_down_left = false;
	glm::vec2 mouse_down_position_left;
	bool mouse_down_right = false;
	glm::vec2 mouse_down_position_right;
	float mouse_wheel_y_previous = 0.0f;

	// Camera control stuff
	glm::vec3 light_position = glm::vec3(0.5f,8.0f,0.5f) ;
	glm::vec3 light_look_at = glm::vec3(0, 0, 0);
	float fov = 1.0f;
	float camera_theta = 0.5f;
	float camera_thi = 0.8f;
	float camera_x_speed = 0.002f;
	float camera_y_speed = -0.002f;
	

};
#endif // #ifndef _FIRST_PERSON_APP_H_