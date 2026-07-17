#ifndef _NARBALL_MENU_APP_H_
#define _NARBALL_MENU_APP_H_ 1

#include "AsyncPlugin.h"
#include "MachineState.h"
#include "VulkanPlugin.h"
#include "PanelPlugin.h"
#include "SteamworksPlugin.h"
#include "glm/glm.hpp"
#include "GLTF.h"
#include "WorldPlugin.h"
#include "ScenePlugin.h"
#include "NarballObjects.h"
#include "Ball.h"
#include "Narwhal.h"

#include <stdio.h>
#include <cstdlib>

#include <string>
#include <map>


namespace Narball{

class NarballMenu : public MachineState, public PanelPlugin::PanelListener, public SteamworksPlugin::SteamEventReceiver {

public:

	static inline int local_player_id = -1; // randomly generated at load time
	static inline int64_t lobby_id = -1 ; // fetched on host or join
	static inline bool has_player = false;
	static inline std::shared_ptr<PanelPlugin::TextBox> player_name ; // if joining mid game, we may need to grab this text from narball game state



	static inline const std::string state_name = "narball_menu_state";
	static inline const int MOUSE_BUTTON_OFFSET = 10000 ; // amount added to mouse button numbers so they can be distinguished from other buttons

	//quickplay server score is ping + offset based on number of players, lower is better
	static inline std::vector<int> player_count_offset = { 40,30,40,0,30,0,30,0,50,50,80,50,80,60,90,70,100, 150 };

	NarballMenu();

	void run() override;

	// Called when switching into this sate before the first time run is called
	void enter(std::shared_ptr<MachineState> from) override;


	// Called when switching outof this state after the last time run is called
	void exit(std::shared_ptr<MachineState> to) override;


	void createMenus();

	void hideLeft(int menu);
	void showLeft(int menu);
	void hideRight(int menu);
	void showRight(int menu) ;

	void createMainMenu();
	void createOptionsMenu();
	void createLobbyMenu();
	void createCreditsMenu();

	void updateLobbyMenu(std::shared_ptr<const Lobby>);


	void enterPanel(int panel) override;

	void exitPanel(int panel) override;

	void enterPanelElement(int panel, int element) override;

	void exitPanelElement(int panel, int element) override;

	void pressPanel(int panel, int element, int button) override;

	void releasePanel(int panel, int element, int button) override;

	int64_t getLobbyID();

	void onSteamGameExternalJoin(std::shared_ptr<SteamworksPlugin::SteamSocket> socket, const SteamworksPlugin::SteamServerInfo& server_info) override ;

private:


	std::chrono::high_resolution_clock::time_point last_run_time;
	std::chrono::high_resolution_clock::time_point current_time;

	int beach_id;

	

	int main_menu = -1;
	int lobby_menu = -1;;
	int credits_menu = -1;
	int options_menu = -1;
	int active_menu = -1;
	PanelPlugin::DefaultInstance left_menu_hide;
	PanelPlugin::DefaultInstance right_menu_hide;
	PanelPlugin::DefaultInstance menu_visible;
	PanelPlugin::DefaultInstance menu_hidden;

	std::map<int, int> start_nav_select ; // where the controller selection start swhen a panel is shown
	glm::vec2 last_controls = glm::vec2(0);
	bool controller_button_held = false;
	bool space_held = false;
	int last_active_pad = -1 ;
	std::chrono::high_resolution_clock::time_point last_control_time = now();


	std::map<std::pair<int,int>, std::shared_ptr<PanelPlugin::MenuButton>> buttons ;

	std::map<std::pair<int, int>, std::shared_ptr<PanelPlugin::TextBox>> text_boxes;
	PanelPlugin::TextBox* selected_text_box = nullptr ;
	PanelPlugin::TextBox* hovered_text_box = nullptr;

	std::string default_join_address = "127.0.0.1";
	std::string default_name = "Narwhal" ;


	std::shared_ptr<PanelPlugin::TextBox> lobby_balls;
	std::shared_ptr<PanelPlugin::TextBox> lobby_points;
	std::vector<int> textbox_button_values = { 1,2,3,4,5,10,20,50,100,200,500, 1000 }; // numeric text boxes scyles these values on a controller button press


	std::shared_ptr<PanelPlugin::Label> options_title;
	std::shared_ptr<PanelPlugin::Label> lobby_title;
	std::shared_ptr<PanelPlugin::Label> lobby_countdown;
	std::shared_ptr<PanelPlugin::Label> lobby_info ;
	std::shared_ptr<PanelPlugin::Label> lobby_result;
	std::shared_ptr<PanelPlugin::MenuButton> join_button ;
	std::shared_ptr<PanelPlugin::MenuButton> quick_join_button;

	std::vector<std::shared_ptr<PanelPlugin::Label>> credit_labels ;

	std::shared_ptr<PanelPlugin::MenuButton> game_start ;
	std::shared_ptr<PanelPlugin::MenuButton> lobby_teams ;
	std::shared_ptr<PanelPlugin::MenuButton> lobby_back ;
	bool game_start_hidden = false ;

	std::shared_ptr<PanelPlugin::Label> music_volume_label ;
	std::shared_ptr<PanelPlugin::Label> music_volume_number;
	int music_volume  = 25 ;
	std::shared_ptr<PanelPlugin::Label> effects_volume_label;
	std::shared_ptr<PanelPlugin::Label> effects_volume_number;
	int effects_volume  = 50 ;
	//TODO menu buttons should NOT stay active when their object is destroyed, so they should be here too
	std::shared_ptr<PanelPlugin::MenuButton> toggle_fps;

	std::map<int, std::shared_ptr<PanelPlugin::Label>> player_label ;





	glm::vec3 camera_position = { 0,12,12.0f }; // above and looking down and slightly forward
	glm::vec3 camera_look_at = { 0,0,0 };
	float fov = 0.35f;
	int menu_width = 1024;
	int menu_height = 1280 ;
	

	std::map<int,bool> last_mouse_down ;


	bool hosting = false;
	bool joined = false ;
	bool leaving = false;
	bool trying_to_quickplay = false;
	std::chrono::high_resolution_clock::time_point leave_time;
	bool waiting_on_join = false;
	bool quick_join_button_spinning = false;
	std::chrono::high_resolution_clock::time_point join_start;
	std::chrono::high_resolution_clock::time_point last_keep_alive ;
	int target_fps = 0; // 0 means vsync

	
};
}// end namespace Narball
#endif // #ifndef _NARBALL_MENU_APP_H_