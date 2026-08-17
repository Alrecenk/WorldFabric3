#ifndef _COLLISION_TEST_APP_H_
#define _COLLISION_TEST_APP_H_ 1

#include "AsyncPlugin.h"
#include "MachineState.h"
#include "Registry.h"

class CollisionTestApp : public MachineState {

public:

	class CollisionShape {
	public:
		virtual glm::vec3 support(const glm::vec3& direction)  = 0;

	};

	class ConvexPolyhedron : public CollisionShape{
	public:
		std::vector<glm::vec3> vertex;
		std::vector<std::vector<int>> face;

		ConvexPolyhedron(){}

		ConvexPolyhedron(const std::vector<glm::vec3>& vertices, const std::vector<std::vector<int>>& faces) ;

		ConvexPolyhedron(std::shared_ptr<ConvexPolyhedron> base, const glm::mat4& pose) ;

		glm::vec3 support(const glm::vec3& direction) override ;

		// Returns an axis aligned bounding box
		static ConvexPolyhedron makeAxisAlignedBox(glm::vec3 min, glm::vec3 max);

		//Alternate form of box that always centers on the origin
		static ConvexPolyhedron makeAxisAlignedBox(glm::vec3 size);

		// Returns a shape for a cylinder with center of ends A and B and the given radius and side count
		static ConvexPolyhedron makeCylinder(glm::vec3 A, glm::vec3 B, float radius, int sides);

		// Returns a shape for a Tetrahedron with the given points
		static ConvexPolyhedron makeTetra(glm::vec3 A, glm::vec3 B, glm::vec3 C, glm::vec3 D);

	};

	class PolyInstance{
	public:
		std::shared_ptr<ConvexPolyhedron> base_shape ;
		std::shared_ptr<ConvexPolyhedron> world_shape;
		
		int scene_id  = -1 ;
		glm::mat4 pose ;

		PolyInstance()  = default ;


		PolyInstance(std::string model, std::shared_ptr<ConvexPolyhedron> base);

		void setPose(glm::mat4& p);
		
		
	};

	static inline const std::string state_name = "collision_test_state";

	CollisionTestApp();

	//Called every frame while the state is active
	void run() override;

	// Called when switching into this state before the first time run is called
	void enter(std::shared_ptr<MachineState> from) override;

	// Called when switching out of this state after the last time run is called
	void exit(std::shared_ptr<MachineState> to) override;

	void updateCamera();


	
	//Point in minkowski difference space
	struct SupportPoint{
		glm::vec3 x ;
		// hold onto points on shapes for use in subsequent steps
		glm::vec3 a, b ;
	};

	//A triangle in monkowski space with a set winding order
	struct SupportTriangle{
		SupportPoint A ;
		SupportPoint B ;
		SupportPoint C ;
		glm::vec3 normal; // normal should be normalize(cross(B - A, C - A))
	};

	//We use edges to build out expanding polytope as points are added
	struct SupportEdge{
		SupportPoint A;
		SupportPoint B;
	};

	//Find the support point of the minkowski difference of two shapes
	//Saves the pointson th shapes themselves for later reconstruction
	SupportPoint findSupportPoint(const glm::vec3 support, const std::shared_ptr<CollisionShape>& A, const std::shared_ptr<CollisionShape>& B) ;

	//Uses GJK to detect whether two convex shapes collide
	//If they collie this returns a simplex in Minkowski diference space enclosing the collision point
	//If they do not collide, this returns an empty vector
	std::vector<SupportTriangle> detectCollision(const std::shared_ptr<CollisionShape>& A, const std::shared_ptr<CollisionShape>& B);

	//Uses expanding polytope algorithm on result of detectCollision
	// Returns a pair containing the deepest collision point followed by a penetration vector to move B out of A
	std::pair<glm::vec3, glm::vec3> getPenetration(std::vector<SupportTriangle>& collision_result, std::shared_ptr<CollisionShape> A, std::shared_ptr<CollisionShape> B) ;


private:

	std::map<std::string, std::shared_ptr<ConvexPolyhedron>> base_shape ;

	std::vector<PolyInstance> instances; // maps scene instance to transform of base shape


	std::vector<glm::vec4> colors = { {1,0,0,0.3}, {0,1,0,0.3},{0,0,1,0.3} };
	std::vector<glm::vec3> positions = { {0,-0.5f,-1}, {0,0,0},{0,-0.5f,1} };

	int light_id = -1; // Scene light
	int mouse_particle_id = -1;
	std::chrono::high_resolution_clock::time_point last_run_time;
	std::chrono::high_resolution_clock::time_point current_time;

	glm::vec3 min = { -3,-4,-3 };
	glm::vec3 max = { 3,4,3 };
	int room_instance_id  = -1 ;


	// Csmera control stuff
	glm::vec3 look_at = glm::vec3(0, 0, 0);
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
	float light_thi = 1.2f;
	float mouse_wheel_y_previous = 0.0f;
};
#endif // #ifndef _COLLISION_TEST_APP_H_