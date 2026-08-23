#ifndef _CONSTRAINT_TEST_APP_H_
#define _CONSTRAINT_TEST_APP_H_ 1

#include "AsyncPlugin.h"
#include "MachineState.h"
#include "Registry.h"
#include "Physics.h"

class ConstraintTestApp : public MachineState {

public:


	class PhysicsCell : Physics::PhysicsContainer {
	public:
		//Bounding box of cell
		glm::vec3 min;
		glm::vec3 max;
		glm::vec3 acceleration = glm::vec3(0,-10,0);
		
		//Contents of cell
		std::unordered_map<int64_t, std::shared_ptr<Physics::RigidBody>> bodies ;
		std::unordered_map<int64_t, std::shared_ptr<Physics::Constraint>> constraints;

		std::unordered_map<int64_t, int> instance ;
		
		int next_ball_id = 1 ;
		int instance_id = -1; // for scene
		static inline const std::string BOX_MODEL = "box" ;
		float box_size = 0.95f ;
		std::shared_ptr<Physics::ConvexPolyhedron> box_shape = std::make_shared< Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeAxisAlignedBox(glm::vec3(box_size, box_size, box_size)));
		float ball_radius = 0.5f;
		std::shared_ptr<Physics::Sphere> ball_shape = std::make_shared<Physics::Sphere>(ball_radius,1.0f);
		static inline const std::string BALL_MODEL = "./Narball/asset/BeachBall.glb";

		float wall_size = 10;
		std::shared_ptr<Physics::ConvexPolyhedron> wall_shape = std::make_shared< Physics::ConvexPolyhedron>(Physics::ConvexPolyhedron::makeAxisAlignedBox(glm::vec3(wall_size, wall_size, wall_size)));
		static inline const std::string WALL_MODEL = "wall";

		PhysicsCell(const glm::vec3& box_min, const glm::vec3& box_max) ;

		//Custom destructor cleans up scene instance
		~PhysicsCell();

		int64_t add(const glm::vec3& pos, const glm::vec3& vel, const glm::vec3& a_vel) ;

		//Ball ids are allocated one after another and are always positive
		Physics::RigidBody* getBody(int64_t id) override;

		//Constraint id is a hash generated with getConstraintID
		Physics::Constraint* getConstraint(int64_t id);

		int64_t getConstraintID(int64_t id1, int64_t id2, int constraint_type){
			return hashBytes(serialize(id1, id2, constraint_type)) ;
		}

		//Finds all collisions of the balls with each other and the walls of the cell
		//Creates or destroys constraints so the contents of constraints matches the current collisions
		//Also sets points and normal for collisions
		void updateCollisions();

		//Convenience method to avoid repeatign code on all walls of the box
		void updateWallCollision(int64_t ball_id, int wall_id, const glm::vec3& point, const glm::vec3& normal, std::unordered_set<int64_t>& found_constraints);

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

	std::chrono::high_resolution_clock::time_point last_ball_time = now();



	glm::vec3 min = { -4,-4,-4 };
	glm::vec3 max = { 4,4,4 };
	float gravity = 4.0f ;
	int millis_between_balls = 300;
	int max_balls = 150 ;

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