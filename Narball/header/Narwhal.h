#ifndef _NARBALL_NARWHAL_H_
#define _NARBALL_NARWHAL_H_ 1


#include "WorldPlugin.h"
#include "ScenePlugin.h"
#include "PanelPlugin.h"
#include "ActionMap.h"
#include "HighlightParticle.h"
#include "Nameplate.h"
#include "NarballObjects.h"
#include "glm/glm.hpp"

#include <stdio.h>
#include <cstdlib>

#include <string>
#include <map>
#include <set>



namespace Narball {


// A Player controlled Narwhal
class Narwhal : public WorldObject {
public:
	glm::vec3 velocity = glm::vec3(0, 0, 0);
	float facing_angle = 0;
	float angular_velocity = 0;
	int color = 0; // 0 is blank, 1 is red, 2 is blue
	int player_id = -1;
	int sound = -1;
	int sound_num = 0;
	int64_t match_id = -1; // allows using grid to find nearby objects efficiently
	std::vector<int64_t> cells;//current cells this is in

	//current player input controls
	glm::vec2 left_stick = glm::vec2(0, 0);
	glm::vec2 right_stick = glm::vec2(0, 0);
	double last_control_time = -1.0;
	int input_num = -1 ;

	static inline float body_radius = 0.22f;
	static inline float horn_length = 0.97f;
	static inline float max_speed = 3.5f;
	static inline float base_acceleration = 1.3f; // base accleration from left stick
	static inline float facing_acceleration = 2.3f; // extra acceleration if facing moving direction goes to 0 at 90 degrees
	static inline float max_turn_speed = 8.2f; // radians per second
	static inline float wall_bounce = 0.5f; // fraction of orthogonal speed retained when bumping the wall
	static inline float ball_body_hit_force = 0.4f; //multiplier for when bumpin the ball with the body
	static inline float ball_horn_hit_force = 1.05f; //multiplier for hittin the ball with the horn
	static inline float narwhal_body_hit_force = 0.8f; //multiplier for when bumpin another narwhal with the body
	static inline float drag = 0.65f; // velocity lost per second
	static inline float speed_for_sound = 0.8f;
	static inline float wall_tail_push_strength = 1.4f; // extra force along facing direction when you bumpawall with your tail
	static inline float min_alignment_for_tail_push = 0.25f; // dot product with wall must be at least this much for tail push

	Narwhal(const glm::vec3& p, int64_t g_id, int player_id);

	//Functions to be used as events must be void return and only const& parameters
	// Also they're not allowed to read or write any data outside the object except through timeline functions
	void update();
	void setVelocity(const glm::vec3& v);
	void setPosition(const glm::vec3& p);
	void applyImpulse(const glm::vec3& impulse, const glm::vec3& hit_point);
	void setControls(const glm::vec2& left_stick, const glm::vec2& right_stick, const int& sdl_input_num);
	void changeColor();
	void tailKickUsed();

	//Functions not used on observables or on read objects need to be const
	void print() const override;

	Narwhal() {} // WorldObject's need a default constructor to make an object to deserialize into
	virtual ~Narwhal() = default; // Force to be polymorphic just in case

	//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
	// Just change the template parameter to match your class
	int getTypeId(Registry* r) const {
		return r->getIdForType<Narwhal>();
	}

};


auto static getStructure(Narball::Narwhal& obj) {
	return std::tie(obj.position, obj.velocity, obj.facing_angle, obj.angular_velocity, obj.match_id, obj.player_id, obj.cells,
		obj.left_stick, obj.right_stick, obj.last_control_time, obj.color, obj.sound, obj.sound_num, obj.input_num);
}

class NarwhalControlAction : public UniversalAction{
public:
	int player_id = -1 ; // player issuing the action
	glm::vec2 left_stick ;
	glm::vec2 right_stick ;
	int input_num ; // used for tracking input latency through the whole system


	NarwhalControlAction(int pid, glm::vec2 l, glm::vec2 r, int n) : player_id(pid),left_stick(l), right_stick(r), input_num(n){};
};

class NarwhalView : public ObjectView<Narwhal>, public virtual ActionReceiver<NarwhalControlAction> {
public:

	int64_t id;
	int scene_id = -1;
	int last_sound = -1;
	int last_sound_number = -1;
	int action_trigger_id = -1 ;
	float butt_angle = 0; // tails drag behind when rotating
	std::chrono::high_resolution_clock::time_point last_sound_time = now();
	std::chrono::high_resolution_clock::time_point last_view_time = now();
	Narwhal last_view;
	double last_age;


	int num_highlight = 0; // Most narwhals have no particles, but the game can fetch the local one and turn this up
	static inline float highlight_size = 0.03f;
	static inline glm::vec4 highlight_color = glm::vec4(0.8f, 1.0f, 0.7f, 0.75f);
	static inline float highlight_spin_radius = Narwhal::body_radius * 1.5f;
	static inline float highlight_lifespan = 2.5f;
	static inline float highlight_spin_speed = 3.0f;
	std::queue<HighlightParticle> highlight_particles;


	static inline std::shared_ptr<const Lobby> lobby; // set externally and used to look up name for nameplate
	static inline bool nameplates_enabled = false ;
	static inline std::string nameplate_font = "arial75";
	static inline glm::vec3 camera_position; // set externally, used to position nameplates
	std::shared_ptr<Nameplate> nameplate;


	//created is called when an objectis observed that ws no observed last time view was called on the world
	void created(std::shared_ptr<const Narwhal>& observation) override;

	//Update is called when an observation is made of an object that was also observed last frame on this same view
	void updated(std::shared_ptr<const Narwhal>& observation) override;

	//Destroyed is called when an observation that was present in the last observation is no longer observed
	//This view will be deleted immediately after this call (it's destructor will be called after this)
	void destroyed() override;

	//HAndles interpolation and extrapolation from the last position to get the ball state "now"
	Narwhal getView(std::shared_ptr<const Narwhal>& observed);

	//Computes the scene pose of a ball
	glm::mat4 computePose(std::shared_ptr<const Narwhal>& nawhal);

	void receiveAction(std::shared_ptr<NarwhalControlAction>& control, std::shared_ptr<ActionTrigger>& trigger) override;
	
};

} // end Narball name space

#endif // #ifndef _NARBALL_NARWHAL_H_