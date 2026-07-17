#ifndef _CHESS_APP_H_
#define _CHESS_APP_H_ 1

#include "AsyncPlugin.h"
#include "MachineState.h"
#include "WorldPlugin.h"
#include "Piece.h"
#include "Board.h"
#include "SteamworksPlugin.h"
#include "ActionMap.h"

namespace Chess {

	class ChessApp : public MachineState, public SteamworksPlugin::SteamEventReceiver {

	public:

		static inline const std::string state_name = "chess_state";
		std::shared_ptr<ChessMouseAction> mouse_action  = std::shared_ptr<ChessMouseAction>(new ChessMouseAction()) ;
		std::chrono::high_resolution_clock::time_point last_moved_mouse = now();

		ChessApp();

		//Called every frame while the state is active
		void run() override;

		// Called when switching into this state before the first time run is called
		void enter(std::shared_ptr<MachineState> from) override;

		// Called when switching out of this state after the last time run is called
		void exit(std::shared_ptr<MachineState> to) override;

		void updateCamera();

		static float raytrace(const glm::vec3& pos, const glm::vec3& ray, int scene_id, glm::mat4 pose);

		void onSteamGameExternalJoin(std::shared_ptr<SteamworksPlugin::SteamSocket> socket, const SteamworksPlugin::SteamServerInfo& server_info) override;

	private:

		void startSteamHosting();
		std::vector<std::string> listFileStems(const std::string& path);
		void registerClassesAndMethods();
		void createWorldAndObjects();
		void setupParticles();
		void setupScene();
		void GlowUpHeldPiece();

		float current_angle = 0;
		int light_effect_id = -1;
		std::chrono::high_resolution_clock::time_point last_run_time;
		std::chrono::high_resolution_clock::time_point current_time;

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
		float light_zoom = 12.0f;
		float light_fov = 1.5f;
		float light_theta_off = 0.4f;
		float mouse_wheel_y_previous = 0.0f;

	};
}
#endif // #ifndef _SCENE_DEMO_APP_H_