#ifndef _CONSTRAINT_TEST_APP_H_
#define _CONSTRAINT_TEST_APP_H_ 1

#include "AsyncPlugin.h"
#include "MachineState.h"
#include "Registry.h"

class ConstraintTestApp : public MachineState {

public:

	class Ball {
	public:
		int64_t id = -1 ;
		glm::vec3 position;
		glm::vec3 velocity;
		glm::vec3 acceleration ;
		float radius = 0.5f;
		float mass = 1.0f;
		float elasticity = 0.5f ; // 0 = inelastic, 1 = full elastic

		// used for rendering
		int instance_id = -1 ;
		static inline const std::string BALL_MODEL = "./Narball/asset/BeachBall.glb" ;

		Ball(int64_t my_id, const glm::vec3& pos , const glm::vec3& vel, const glm::vec3& acc);

		void integrateVelocity(float dt);

		void integrateAcceleration(float dt);

		void updateGraphics();

		//Custom destructor cleans up scene instance
		~Ball();

	};

	class Constraint {
	public:
		//Update the constraint terget based on velocity at the start of the rame
		virtual void updateConstraintTarget() = 0;

		//Apply a starting impulse carried over if this constraint has existed ofr mutliple frames in a row
		virtual void applyWarmingImpulse() = 0;

		//Applies impulse to velocity of involved bodies to satisfy this constraint
		virtual void applyConstraint() = 0;
	};

	class BallCollision : public Constraint {
	public:
		int64_t id1 = -1;
		int64_t id2 = -1;
		glm::vec3 warm_impulse;
		glm::vec3 point ;
		glm::vec3 normal ;
		float target = 0 ;

		static inline float collision_bias = 0.01f;
		static inline float min_velocity_for_elastic = 0.01f;

		void updateConstraintTarget() override;
		void applyWarmingImpulse() override;
		void applyConstraint() override;
	};

	class BallWallCollision : public Constraint {
	public:
		int64_t id ;
		glm::vec3 warm_impulse;
		glm::vec3 point ;
		glm::vec3 normal ;
		float target = 0 ;

		static inline float collision_bias = 0.01f;
		static inline float min_velocity_for_elastic = 0.01f ;

		void updateConstraintTarget() override;
		void applyWarmingImpulse() override;
		void applyConstraint() override;
	};
		

	class PhysicsCell {
	public:
		//Bounding box of cell
		glm::vec3 min;
		glm::vec3 max;

		//Contents of cell
		std::map<int64_t, std::shared_ptr<Ball>> balls ;
		std::map<int64_t, std::shared_ptr<Constraint>> constraints;
		
		int next_ball_id = 1 ;
		int instance_id = -1; // for scene
		static inline const std::string BOX_MODEL = "box" ;

		PhysicsCell(const glm::vec3& box_min, const glm::vec3& box_max) ;

		//Custom destructor cleans up scene instance
		~PhysicsCell();

		int64_t addBall(const glm::vec3& pos, const glm::vec3& vel, const glm::vec3& acc) ;

		//Ball ids are allocated one after another and are always positive
		std::shared_ptr<Ball> getBall(int64_t id);

		//Constraint id is a hash generated with getConstraintID
		std::shared_ptr<Constraint> getConstraint(int64_t id);

		int64_t getConstraintID(int id1, int id2, int constraint_type){
			return hashBytes(serialize(id1, id2, constraint_type)) ;
		}

		int64_t getConstraintID(int id1, int constraint_type) {
			return hashBytes(serialize(id1, constraint_type));
		}

		//Finds all collisions of the balls with each other and the walls of the cell
		//Creates or destroys constraints so the contents of constraints matches the current collisions
		void updateCollisions();

		//Run physics forward one frame
		void runPhysicsFrame(float dt, int constraints_iter);

		//Calls update graphics on all the balls
		//Also renders the box
		void updateGraphics();

		
	};

	static inline const std::string state_name = "constraint_test_state";

	ConstraintTestApp();

	//Called every frame while the state is active
	void run() override;

	// Called when switching into this state before the first time run is called
	void enter(std::shared_ptr<MachineState> from) override;

	// Called when switching out of this state after the last time run is called
	void exit(std::shared_ptr<MachineState> to) override;

	void updateCamera();

private:

	std::shared_ptr<PhysicsCell> cell ;
	int light_id = -1; // Scene light
	int mouse_particle_id = -1;
	std::chrono::high_resolution_clock::time_point last_run_time;
	std::chrono::high_resolution_clock::time_point current_time;


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
	float light_thi = 1.2f ;
	float mouse_wheel_y_previous = 0.0f;
};
#endif // #ifndef _CONSTRAINT_TEST_APP_H_