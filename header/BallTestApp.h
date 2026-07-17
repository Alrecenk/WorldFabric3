#ifndef _BALL_TEST_APP_H_
#define _BALL_TEST_APP_H_ 1

#include "AsyncPlugin.h"
#include "Variant.h"
#include "AudioPlugin.h"
#include "MachineState.h"
#include "ParticlePlugin.h"
#include "glm/glm.hpp"
#include "Timeline.h"

#include "VulkanDemoApp.h"

#include <stdio.h>
#include <cstdlib>

#include <string>
#include <map>
#include <set>




class BallTestApp : public MachineState {
public:

	struct MeshPushConstants {
		glm::mat4 world_matrix;
		alignas(16) glm::vec3 camera_position;
		VkDeviceAddress vertexBuffer;
		VkDeviceAddress instanceBuffer;
	};

	struct ComputePushConstants {
		glm::mat4 world_matrix;
		alignas(16) glm::vec3 camera_position;
		VkDeviceAddress component_buffer;
		float test_value;
	};


	struct ComputeComponent {
		glm::vec4 some_data;
	};

	// A bouncing ball example made to work in the World Fabric Timeline
	class Ball : public Timeline::WorldObject {
	public:
		glm::vec3 velocity = glm::vec3(0, 0, 0);
		float radius = 0.5f;
		glm::vec3 color = glm::vec3(1.0f, 0, 0);
		int64_t grid_id = -1;
		static inline float tick_interval = 1.0f/60.0f;
		bool player = false;
		std::vector<int64_t> cells;//current cells this ball is in
		

		Ball(const glm::vec3& p, const glm::vec3& v, float r, const glm::vec3& c,int64_t g);

		//Functions to be used as events must be void return and only const& parameters
		// Also they're not allowed to read or write any data outside the object except through timeline functions
		void update();
		void setVelocity(const glm::vec3& v);
		void setPosition(const glm::vec3& p);
		void applyImpulse(const glm::vec3& impulse);

		void print() const override;

		Ball() {} // WorldObject's need a default constructor to make an object to deserialize into
		virtual ~Ball() = default; // Force to be polymorphic just in case

		//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
		// Just change the template paramter to match your class
		int getTypeId(Registry* r) const {
			return r->getIdForType<Ball>();
		}

		//ids for functons generated when they get registered
		//don't actually have to save these but it's helpful for debugging
		static inline int UPDATE = -1;
		static inline int SET_VELOCITY = -1;
		static inline int SET_POSITION = -1;
		static inline int APPLY_IMPULSE = -1;
	};

	class Cell : public Timeline::WorldObject {
	public:

		//glm::vec3 min = glm::vec3(0, 0, 0);// min and max define an axis aligned bounding box of the extent of this cell
		//glm::vec3 max = glm::vec3(0, 0, 0);
		std::set<int64_t> balls; //ids of balls in the cell

		Cell(glm::vec3 mn, glm::vec3 mx);

		//Event functions
		// Make a ball in the cell
		//void createBall(const glm::vec3& p, const glm::vec3& v, const float& r);

		//Add or remove a reference to a ball in this cell
		void addBall(const int64_t& ball_id);
		void removeBall(const int64_t& ball_id);

		void print() const override;

		Cell() {} // WorldObject's need a default constructor to make an object to deserialize into
		virtual ~Cell() = default; // Force to be polymorphic just in case

		//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
		// Just change the template paramter to match your class
		int getTypeId(Registry* r) const{
			return r->getIdForType<Cell>();
		}
		//ids for functons generated when they get registered
		//don't actually have to save these but it's helpful for debugging
		//static inline int CREATE_BALL = -1;
		static inline int ADD_BALL = -1;
		static inline int REMOVE_BALL = -1;
	};

	class Grid : public Timeline::WorldObject {
	public:
		glm::vec3 min = glm::vec3(0,0,0);// min and max define an axis aligned bounding box of the extent of this entire grid
		glm::vec3 max = glm::vec3(0,0,0);
		int width = 0;
		int height = 0;
		std::vector<std::vector<int64_t>> grid; // ids of cells

		Grid(glm::vec3 mn, glm::vec3 mx, int w, int h);

		// Build the grid
		void createGrid();

		//Create a ball in the grid
		//void createBall(const glm::vec3& p, const glm::vec3& v, const float& r);

		void print() const override;

		//Get the ids of the cells a ball intersects (not an event, the ball update event can call this beause it is const)
		std::vector<int64_t> getCells(const glm::vec3& center, float radius) const;

		//returns the cell a point is in
		int64_t getCell(const glm::vec3& p) const;

		Grid() {} // WorldObject's need a default constructor to make an object to deserialize into
		virtual ~Grid() = default; // Force to be polymorphic just in case

		//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
		// Just change the template paramter to match your class
		int getTypeId(Registry* r) const {
			return r->getIdForType<Grid>();
		}
		//ids for functons generated when they get registered
		//don't actually have to save these but it's helpful for debugging
		static inline int CREATE_GRID = -1;
		//static inline int CREATE_BALL = -1;
	};


	static inline const std::string state_name = "ball_test_state";
	static inline int host_transform_group = 5;
	static inline int client_transform_group = 6;
	static inline glm::vec2 min_floor = glm::vec2(-5, -5);
	static inline glm::vec2 max_floor = glm::vec2(5, 5);
	static inline float ball_radius = 0.12f;
	static inline int num_balls = 500 ;
	static inline int grid_size = 10 ;
	static inline float ball_speed = 1.5f;
	static inline float player_speed = 3.0f;
	static inline float player_radius = 0.3f;
	static inline int ping = 50;


	float max_info_speed = 30.0f; // blocks/second, lower is more time warp, which hides latency better and limits rollback but delays events and requires more history
	float min_event_duration = 1.0f/600.0f; // longer makes getruntime call less but limits event rate on single objects in timeline
	float history_kept = 7.5f;// longer allows more time warp and real latency, but impacts performance
	float sync_age = 2.5f;
	float sync_depth = 2.5f;

	int64_t client_player_id = -1;
	int64_t host_player_id = -1;

	int64_t grid_id = -1;


	//Loads models from the hard drive on construction
	BallTestApp();

	void run() override;

	// Called when switching into this sate before the first time run is claled
	void enter(std::shared_ptr<MachineState> from) override;


	// Called when switching outof this state after the last time run is called
	void exit(std::shared_ptr<MachineState> to) override;

	int createBlock(VulkanPlugin* scene, int transform_group, glm::vec3 min, glm::vec3 max);

	void updateParticles(Timeline& timeline, glm::vec3 & vantage, double time, int transform_group, std::unordered_map<int64_t, int>& ball_particle, ParticlePlugin* particles);

	// runs test to make sure the timeline is behaving properly
	static void testTimeline();

	void checkBaseEvents(double time);
	void checkBaseObjects(double time);

	void createBall(double time);


private:




	std::shared_ptr<Registry> registry = std::make_shared<Registry>();
	Timeline host_timeline = Timeline(registry,max_info_speed, min_event_duration, 20, history_kept);
	glm::vec3 host_vantage = glm::vec3(0, 0, 0);
	double host_time = 0;
	glm::vec3 last_host_valocity = glm::vec3(0, 0, 0);
	


	Timeline client_timeline = Timeline(registry, max_info_speed, min_event_duration, 20, history_kept);
	glm::vec3 client_vantage = glm::vec3(0, 0, 0);
	double client_time = 0;
	glm::vec3 last_client_valocity = glm::vec3(0, 0, 0);
	

	std::unordered_map<int64_t, int> host_ball_particle; // maps timeline id to particle instance
	std::unordered_map<int64_t, int> client_ball_particle; // maps timeline id to particle instance

	int host_floor_instance = -1;
	int client_floor_instance = -1;
	std::vector<int> wall_instances;
	std::map<std::string, std::pair<std::vector<std::shared_ptr<TriangleModel<VulkanDemoApp::MeshPushConstants, GLTF::BufferVertex, GLTF::Instance256>>>, std::shared_ptr<GLTF>>> meshes;
	std::shared_ptr<TriangleShaderProgram> mesh_program;
	std::shared_ptr<ScreenShaderProgram>  postprocess_shader;
	std::shared_ptr<ScreenModel<ComputePushConstants, ComputeComponent>> post_effect;
	int box_model_id;
	int post_effect_id;
	glm::mat4 host_transform ;
	glm::mat4 client_transform ;


	bool space_held = false;
	bool enter_held = false;

	std::string font = "arial50";

	glm::vec3 view_position = glm::vec3(0, 1, 0);
	glm::vec3 view_Z = glm::vec3(0, 0, 1);

	std::chrono::high_resolution_clock::time_point last_run_time = now();


	int sync_state = 0; // 0 means not synced, 1 means host sending , 2 means client sending
	std::chrono::high_resolution_clock::time_point last_sync_time = now();
	
	Timeline::UpdatePacket client_update;
	Timeline::UpdatePacket host_update;

};


// Returns the structure of the class data as a reference tuple
// Note: getStructure implementations must be auto static and in global name space, so the templated serializer can find them
// Don't forget to include position for TimelineObjects
auto static getStructure(BallTestApp::Ball& obj) {
	return std::tie(obj.position, obj.radius, obj.velocity, obj.grid_id, obj.cells, obj.color, obj.player);
}

auto static getStructure(BallTestApp::Cell &obj) {
	return std::tie(obj.position, obj.balls);
}

auto static getStructure(BallTestApp::Grid& obj) {
	return std::tie(obj.position, obj.min, obj.max, obj.width, obj.height, obj.grid);
}

#endif // #ifndef _BALL_TEST_APP_H_ 