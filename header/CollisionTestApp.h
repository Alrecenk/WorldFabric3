#ifndef _COLLISION_TEST_APP_H_
#define _COLLISION_TEST_APP_H_ 1

#include "AsyncPlugin.h"
#include "MachineState.h"
#include "Registry.h"
#include "Utilities.h" // used for glm::vec3 hash

class CollisionTestApp : public MachineState {

public:

	class CollisionShape {
	public:
		virtual glm::vec3 support(const glm::vec3& direction)  = 0;

	};

	class Point : public CollisionShape {
	public:
		glm::vec3 x ;
		int particle_id = -1 ;
		Point(const glm::vec3& p): x(p) {}

		glm::vec3 support(const glm::vec3& direction) override{
			return x ;
		}
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


	static inline const int MAX_GJK_ITERATIONS = 10;
	//Point in minkowski difference space
	struct SupportPoint{
		glm::vec3 x ;
		// hold onto points on shapes for use in subsequent steps
		glm::vec3 a, b ;

		//Overload linear operators to allow manipulation in barycentric coordinates
		SupportPoint operator*(const float& scale){
			return {x*scale, a*scale, b*scale} ;
		}

		SupportPoint operator+(const SupportPoint& o) {
			return { x +o.x, a + o.a, b +o.b };
		}
	};

	//A triangle in monkowski space with a set winding order
	struct SupportTriangle{
		SupportPoint A ;
		SupportPoint B ;
		SupportPoint C ;
		glm::vec3 normal; // normal should be normalize(cross(B - A, C - A))
		float d = 0 ; // normal * x + d > 0 means in front of the plane

		SupportTriangle(const SupportPoint& a,const  SupportPoint& b,const  SupportPoint& c) : A(a), B(b), C(c){
			normal = glm::normalize(glm::cross(B.x-A.x, C.x-A.x)) ;
			d = -glm::dot(normal,A.x);
		}

		float signedDistance(const glm::vec3& p){
			return glm::dot(normal, p) + d ;
		}	
	};


	
	//We use edges to build out expanding polytope as points are added
	struct SupportEdge{
		SupportPoint A;
		SupportPoint B;
		bool disabled = false;
	};

	
	

	//Find the support point of the minkowski difference of two shapes
	//Saves the points on the shapes for later reconstruction
	SupportPoint findSupportPoint(const glm::vec3 direction, const std::shared_ptr<CollisionShape>& A, const std::shared_ptr<CollisionShape>& B) ;

	//Build a support simplex from a triangle facing a point
	std::vector<CollisionTestApp::SupportTriangle> buildSUpportSimplex(const CollisionTestApp::SupportTriangle& triangle, const CollisionTestApp::SupportPoint& D);

	//Uses GJK to detect whether two convex shapes collide
	//If they collide this returns a simplex in Minkowski diference space enclosing the collision point
	//If they do not collide, this returns an empty vector
	std::vector<SupportTriangle> detectCollision(const std::shared_ptr<CollisionShape>& A, const std::shared_ptr<CollisionShape>& B);



	//Adds an edge fromed by the two support points to an edge list or disables an inner edge on duplication (used in getPenetration)
	void countEdge(const SupportPoint& A, const SupportPoint& B, std::vector<SupportEdge>& edge_list);


	//Uses expanding polytope algorithm on result of detectCollision
	// Returns a supportPoint containg the resoltuion vector in x and the closets points on the shapes in a and b
	SupportPoint getPenetration(std::vector<SupportTriangle>& collision_result,const std::shared_ptr<CollisionShape>& A,const std::shared_ptr<CollisionShape>& B) ;



	

private:

	std::map<std::string, std::shared_ptr<ConvexPolyhedron>> base_shape ;

	std::vector<PolyInstance> instances; // maps scene instance to transform of base shape

	


	std::vector<glm::vec4> colors = { {1,0,0,0.3}, {0,1,0,0.3},{0,0,1,0.3} };
	std::vector<glm::vec3> positions = { {0,0.6-0.1,-0.2}, {0,0.6,0},{0,0.6-0.1,0.2} };
	float particle_size = 0.003f ;
	int light_id = -1; // Scene light
	int mouse_particle_id = -1;
	std::chrono::high_resolution_clock::time_point last_run_time;
	std::chrono::high_resolution_clock::time_point current_time;

	glm::vec3 min = { -1.5,0,-1.5};
	glm::vec3 max = { 1.5,2.5,1.5 };
	int room_instance_id  = -1 ;

	bool show_grid = false;
	std::vector<std::shared_ptr<Point>> points;

	std::vector<int> display_particles;
	std::vector<int> last_display_particles;

	// Csmera control stuff
	glm::vec3 look_at = glm::vec3(0, 0.6f, 0);
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
	float zoom = 2.5f;
	float light_zoom = 2.0f;
	float light_fov = 1.0f;
	float light_theta = 0.4f;
	float light_thi = 1.2f;
	float mouse_wheel_y_previous = 0.0f;

};

#endif // #ifndef _COLLISION_TEST_APP_H_