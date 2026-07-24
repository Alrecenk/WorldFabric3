#ifndef _NARBALL_BALL_H_
#define _NARBALL_BALL_H_ 1


#include "WorldPlugin.h"
#include "ScenePlugin.h"
#include "PanelPlugin.h"
#include "glm/glm.hpp"
#include <stdio.h>
#include <cstdlib>

#include <string>
#include <map>
#include <set>

namespace Narball {


class Ball : public WorldObject {
public:
	glm::vec3 velocity = glm::vec3(0, 0, 0);
	int64_t match_id = -1; // allows using grid to find nearby objects efficiently
	std::vector<int64_t> cells;//current cells this is in

	//Track sound plays
	int sound = -1; // last sound played
	int sound_num = 0; // how many sounds have played (used to tell if sound is a new sound)

	//yrack the last player id to touch the ball on each team for determining who scored
	int last_touch_red = 0;
	int last_touch_blue = 0;

	bool asleep = false; // Balls go to sleep when not moving to save processing

	static inline float max_speed = 6.0f;
	static inline float drag = 2.8f; // how much velocity is lost per second ;
	static inline float ball_ball_hit_force = 0.7f; //multiplier for when balls hit each other
	static inline float wall_bounce = 0.85f; //multiplier for when balls hit walls
	static inline float speed_for_sound = 1.0f; // how balls have ot be moving to generate sound effects

	Ball(const glm::vec3& p, int64_t g_id);

	//Functions to be used as events must be void return and only const& parameters
	// Also they're not allowed to read or write any data outside the object except through timeline functions
	void update();
	void setVelocity(const glm::vec3& v);
	void setPosition(const glm::vec3& p);
	void applyImpulse(const glm::vec3& impulse, const glm::vec3& hit_point);
	void narwhalHit(const glm::vec3& impulse, const glm::vec3& hit_point, const int& player_id, const bool& red_team); // same as apply impulse but tracks hitter for scoring
	void destroy();

	//general function on the same object can be called within events as long as they don't break the data rules
	void resetBall();

	//Functions used on observables or on read objects need to be const
	void print() const override;

	Ball() {} // WorldObject's need a default constructor to make an object to deserialize into
	virtual ~Ball() = default; // Force to be polymorphic just in case

	//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
	// Just change the template parameter to match your class
	int getTypeId(Registry* r) const {
		return r->getIdForType<Ball>();
	}

};


class BallView : public ObjectView<Ball> {
public:
	int64_t id;
	int scene_id = -1;
	int last_sound = -1;
	int last_sound_number = -1;
	std::chrono::high_resolution_clock::time_point last_sound_time = now();
	Ball last_view;
	double last_age;
	bool cleaned = false ;

	//created is called when an objectis observed that ws no observed last time view was called on the world
	void created(const Ball& observation) override;

	//Update is called when an observation is made of an object that was also observed last frame on this same view
	void updated(const Ball& observation) override;

	//Destroyed is called when an observation that was present in the last observation is no longer observed
	//This view will be deleted immediately after this call (it's destructor will be called after this)
	void destroyed() override;


	//HAndles interpolation and extrapolation from the last position to get the ball state "now"
	Ball getView(const Ball& observed);

	//Computes the scene pose of a ball
	glm::mat4 computePose(const Ball& ball);

	~BallView(){
		if(!cleaned){
			printf("Ball view has been orphaned!\n");
		}
	}
};

auto static getStructure(Narball::Ball& obj) {
	return std::tie(obj.position, obj.velocity, obj.match_id, obj.cells, obj.sound, obj.sound_num, obj.asleep, obj.last_touch_red, obj.last_touch_blue);
}


} // end Narball name space

#endif // #ifndef _NARBALL_BALL_H_