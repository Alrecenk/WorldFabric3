#ifndef _NARBALL_OBJECTS_H_
#define _NARBALL_OBJECTS_H_ 1


#include "WorldPlugin.h"
#include "ScenePlugin.h"
#include "PanelPlugin.h"
#include "glm/glm.hpp"
#include <stdio.h>
#include <cstdlib>

#include <string>
#include <map>
#include <set>

namespace Narball{

const inline std::string NARBALL = "narball"; // world name used for Nartball
const inline std::string narwhal_model[] = { "narwhal", "red", "blue" };
const inline std::string tail_animation = "tail" ;
const inline std::string left_fin_animation = "left_fin";
const inline std::string right_fin_animation = "right_fin";
const inline std::string butt_bone = "butt" ; // this needs to match the bone name in the armature in the GLB to grab the right bone for IK
inline int butt_bone_id = -1 ;
const inline std::string ball_model = "beach_ball";
const inline std::string pool_model = "pool";
const inline std::string beach_model = "beach";

const inline std::string NARBALL_VERSION = "v0.098" ;
const inline std::string NARBALL_DEDICATED_VERSION = "1.0.0.0"; // Needs to match the Steamworks setting to make servers visible


inline int BALL_HIT_SOUND = -1 ;
inline int NARWHAL_HIT_SOUND = -1;
inline int WALL_HIT_SOUND = -1;
inline int BALL_BALL_SOUND = -1;
inline int BALL_WALL_SOUND = -1; // These are set in main when the sounds are loaded
inline int SCORE_SOUND = -1;
inline int COUNTDOWN_SOUND = -1;
inline int ENDING_SOUND = -1;
inline int TAP_SOUND = -1;
inline int MENU_MUSIC = -1;
inline int MATCH_MUSIC = -1 ;
inline int MUSIC_SOURCE = -1;

const inline int MUSIC_GROUP = 1;
const inline int SOUND_EFFECTS_GROUP = 2;

const inline float NARBALL_MAX_INFO_SPEED = 100.0f ;
const inline float NARBALL_MIN_EVENT_DURATION = 0.0003f ;
const inline float NARBALL_MAX_READ_DISTANCE = sqrtf(16*16+9*9)*0.6f ; // Slightly more than half the diagonal of the field
const inline float MATCH_UPDATE_TICK_INTERVAL = 0.5f ;// How often the match checks if it is won
const inline float MAX_RESPAWN_X_SPEED = 1.0f ; // how much x velocity a ball could have after respawning
inline std::unordered_map<int, std::pair<float,float>> sound_pitch_range ; // range of pitches for a sound for each activity

inline int keep_alive_interval = 400; // milliseconds of real time
inline double kick_interval = 2.0; // seconds of game time


inline glm::vec2 min_arena = glm::vec2(-8, -4.5);
inline glm::vec2 max_arena = glm::vec2(8, 4.5);
inline float bob_rate = 1.5f; // how fast objects bob up and down in the water in addition to the water movement
inline float bob_magnitude = 0.06f; // how much objects bob up and down in the water in addition to the water movement
inline float narwhal_tilt = 0.2f; // amount to tilt the narwhal so the horn is more likely to be above water
inline float narwhal_lift = -0.02f; // amount to lift the narwhal to be more above water
inline float butt_spin_rate = 8.0f; // how fast the butt rotates away from the model while spinning
inline float max_butt_spin = 0.8f; // max butt misalignment from spinning in place
inline float control_delay = 1.0f / 70.0f; // how much to delay a player's controls (more hurts responsiveness but reduces apparent network lag and CPU load) 
inline bool fancy_interpolation = true;
inline float view_fudge_factor = 1.4f;
inline int local_narwhal_highlight_particles = 15 ; // number of scout flies for the local narwhal highlight

//These vary based on number of balls
inline float tick_interval = 1.0f / 60.0f;
inline float ball_radius = 0.2f;

inline glm::vec4 water_flow ;// current flow velocity of the water wave, shuld be set to match what is passed to the shader
inline glm::quat butt_base_rotation; // default butt pose on the narwhal model (set when generating butt IK pins)

//This water height function is to bob objects up and down with the water flow
//It needs to matcthe correspond water height in the water post processing shader
inline float waterHeight(float x, float z, float time, const glm::vec4& flow) {
	return (float)((sin(x * 0.75 + time * flow.x) + sin(z + time * flow.y)) * 0.15f);
	//return 0 ;
}

class Cell : public WorldObject {
public:

	std::set<int64_t> contents; //ids of things in the cell

	Cell(glm::vec3 mn, glm::vec3 mx);


	//Add or remove a reference to an object in this cell
	void add(const int64_t& id);
	void remove(const int64_t& id);

	void destroy();

	void destroyBalls();

	void print() const override;

	Cell() {} // WorldObject's need a default constructor to make an object to deserialize into
	virtual ~Cell() = default; // Force to be polymorphic just in case

	//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
	// Just change the template parameter to match your class
	int getTypeId(Registry* r) const {
		return r->getIdForType<Cell>();
	}

};

class Match : public WorldObject {
public:
	glm::vec3 min = glm::vec3(0, 0, 0);// min and max define an axis aligned bounding box of the extent of this entire grid
	glm::vec3 max = glm::vec3(0, 0, 0);
	int width = 0; // width and height define number of cells in grid
	int height = 0;
	float ball_radius = 0.2f ;
	float tick_interval  = 1.0f/60.0f;
	double start_time = 0.0 ;
	std::vector<std::vector<int64_t>> grid; // ids of cell objects
	bool ready = false;
	static inline float goal_size = 2.0f;
	int left_score = 0;
	int right_score = 0;
	int64_t lobby_id  = -1 ;

	Match(int64_t lobby_id, glm::vec3 mn, glm::vec3 mx, int w, int h, float tick_interval, double start_time, float ball_radius);

	// Build the grid
	void createMatch();

	void scorePoints(const int& left, const int& right);

	void update();

	void closeMatch();

	void print() const override;

	//Get the ids of the cells a circle intersects (not an event, the ball update event can call this because it is const)
	std::vector<int64_t> getCells(const glm::vec3& center, float radius) const;

	//returns the cell a point is in
	int64_t getCell(const glm::vec3& p) const;

	//returns the grid coordinates of a cell a point is in
	std::pair<int, int> getMatchCoord(const glm::vec3& p) const;

	Match() {} // WorldObject's need a default constructor to make an object to deserialize into
	virtual ~Match() = default; // Force to be polymorphic just in case

	//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
	// Just change the template parameter to match your class
	int getTypeId(Registry* r) const {
		return r->getIdForType<Match>();
	}

};


struct LobbyPlayer{
	int number  = -1; // generated at random when the app starts to identify a player
	bool team = false; // red = true, blue = false
	double last_keep_alive = 0; // time of last keep alive action
	std::string name = "Narwhal" ; // display name
	int score = 0 ; // How many points player has scored
};

class Lobby : public WorldObject {
public:
	int balls = 0;
	int match_points =0;
	std::string result = "First match";
	std::map<int, LobbyPlayer> players;
	double time_lobby_started = 0;
	float min_time_in_lobby = 0;


	static inline const int OPEN = 0 ; // Lobby open and accepting conections
	static inline const int CLOSED = 1; // Lobby is closed, the host is probably shutting down
	static inline const int STARTED = 2; // The match has started (includes countdown)
	static inline const int ENDING = 3 ; //The match has ended, shows the score, will return to open soon
	
	int state = OPEN ; 

	static inline double TIME_AT_END_SCREEN = 5 ; // Time the end screen displays
	double last_state_change_time = 0 ; // last time state changed in world seconds
	
	Lobby(int num_balls, int num_points) ;

	void setMatchParameters(const int& num_balls, const int& num_points);

	void addPlayer(const int& player_id, const std::string& name) ;

	void removePlayer(const int& player_id) ;

	void setTeam(const int& player_id, const bool& team) ;

	void rewardPoint(const int& player_id);

	void switchTeam(const int& player_id);

	void setState(const int& state);

	void setResult(const std::string& r);

	void keepAlive(const int& player_id);

	void kickDisconnected();

	void setTimeLobbyStarted(const double& time_lobby_started);

	void setMinTimeInLobby(const float& min_time_in_lobby);


	void print() const override;

	Lobby() {} // WorldObject's need a default constructor to make an object to deserialize into
	virtual ~Lobby() = default; // Force to be polymorphic just in case

	//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
	// Just change the template parameter to match your class
	int getTypeId(Registry* r) const {
		return r->getIdForType<Lobby>();
	}

};


// The parameters for the the Narball post processintg shader
struct NarballLightComponent {
	glm::vec4 light_position;
	glm::vec4 light_color;
	glm::mat4 light_matrix;
	glm::vec4 water_color = glm::vec4(0.5f, 0.5f, 1.0f, 0.5f);
	glm::vec4 foam_color = glm::vec4(0.8f, 0.8f, 1.0f, 1.0f);
	glm::vec4 caustic_color = glm::vec4(0.45f, 0.45f, 0.9f, 1.0f);
	glm::vec4 flow_velocity = glm::vec4(0.12, 0.15, 1.0, 0); // x,z, foam, shadow
	float ambient_brightness = 0.7f;
	float time = 0.0;
};


inline int debug_panel = -1;
inline std::shared_ptr<PanelPlugin::Label> debug_label ;
inline int debug_frames = 0 ;
inline float total_dt = 0 ;
inline int debug_fps = 0 ;
inline bool debug_panel_enabled = false;
void updateDebugPanel();


auto static getStructure(Narball::Cell& obj) {
	return std::tie(obj.position, obj.contents);
}

auto static getStructure(Narball::Match& obj) {
	return std::tie(obj.position, obj.min, obj.max, obj.width, obj.height, obj.grid, obj.ready, obj.left_score, obj.right_score, obj.lobby_id, obj.tick_interval, obj.ball_radius, obj.start_time);
}

auto static getStructure(Narball::LobbyPlayer& obj) {
	return std::tie(obj.number, obj.team, obj.last_keep_alive, obj.name, obj.score);
}

auto static getStructure(Narball::Lobby& obj) {
	return std::tie(obj.position, obj.balls, obj.match_points, obj.players, obj.state, obj.last_state_change_time, obj.result, obj.time_lobby_started, obj.min_time_in_lobby);
}

} // end Narball name space

#endif // #ifndef _NARBALL_OBJECTS_H_