#ifndef _GLTF_H_
#define _GLTF_H_ 1

#include "Variant.h"
#include "OptimizationProblem.h"
#include "TableInterface.h"
#include "VulkanPlugin.h" 

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/ext.hpp"

#include <set>


typedef Variant::Type Type;
typedef unsigned int uint;

class GLTF : public OptimizationProblem, public TableInterface {

    public:

        struct Accessor{
            std::string type;
            uint component_type=0;
            Variant data;
        };

        struct Vertex{
            glm::vec3 position = {0, 0, 0}; // position in global space
            glm::vec3 normal = {0, 0, 0}; // normal in global space
            glm::vec2 tex_coord = {0, 0};
            glm::vec4 color_mult = {1.0f, 1.0f, 1.0f, 1.0f};

            glm::ivec4 joints = {0,0,0,0}; // Nodes this vertex is skinned to if any
            glm::vec4 weights = {0,0,0,0}; // weights for each skinning node
            
            glm::vec3 transformed_position = { 0,0,0 }; // position in linear skin local space
            glm::vec3 transformed_normal = { 0,0,0 }; //notmal in linjear skin local space
            
			std::vector<glm::vec3> morph_position ; // index here aligns with morph names on the GLTF
			std::vector<glm::vec3> morph_normal ;
        };

        struct Triangle{
            int A;
            int B;
            int C;
            int material = -1;
        };

        struct Material{
            std::string name ="";
            bool double_sided = false;
            glm::vec3 color = {1.0f, 1.0f, 1.0f};
            float metallic = 1.0f;
            float roughness = 1.0f;
            bool texture = false; 
            int image = 0 ;
        };
            
        struct Image{
            std::string name = "";
            int width = 0 ;
            int height = 0 ;
            int channels = 0 ;
            Variant data; // byte array
        };

        struct Node{
            std::string name="" ;
            int parent = -1;
            std::vector<int> children ;
            // Local transform is in components
            glm::quat rotation = {1.0f, 0.0f, 0.0f, 0.0f};
            glm::vec3 scale = {1.0f,1.0f,1.0f};
            glm::vec3 translation = {0.0f, 0.0f, 0.0f};

            glm::quat base_rotation = {1.0f, 0.0f, 0.0f, 0.0f};
            glm::vec3 base_scale = {1.0f,1.0f,1.0f};
            glm::vec3 base_translation = {0.0f, 0.0f, 0.0f};

            glm::mat4 mesh_to_bone ; //transform from starting coordinates into bone space
            glm::mat4 bone_to_mesh ;
            glm::mat4 transform ; // combines model position, inverse, and current node transform for vertex manipulation from base coordinates
            glm::mat4 bone_to_model ; // maps coordinate in bone space to model's space
			float stiffness = 1.0f ; // How hard this node is to move with IK
            float animation_weight = 1.0f; // When a pose is generated from an animation this is how much the animation affected this bone
        };

        enum Path {ROTATION, TRANSLATION, SCALE};

        struct AnimationChannel{
            int node =-1;
            Path path = SCALE;
            std::vector<std::pair<float,glm::vec4>> samples ;
            int last_read = 0 ;
        };

        struct Animation{
            std::string name = "";
            float duration = 0;
            std::vector<AnimationChannel> channels;
        };

        struct Pin{
            std::string name = "";
            int bone ;
            glm::vec3 local_point;
            glm::vec3 target;
            float weight = 1.0f ;
            glm::quat rot_target;
            float rot_weight = 1.0f ;
        };

		// A compacted Vertex struct specifically for fficient use in GPU buffers
		struct alignas(16) BufferVertex{
			alignas(16) glm::vec3 position = { 0, 0, 0 }; // position in global space
			alignas(16) glm::vec3 normal = { 0, 0, 0 }; // normal in global space
			alignas(16) glm::vec4 color = { 1, 1, 1, 1 };
			alignas(16) glm::vec2 tex_coord = { 0, 0 };

			alignas(16) glm::ivec4 joints = { 0,0,0,0 }; // Nodes this vertex is skinned to if any
			alignas(16) glm::vec4 weights = { 0,0,0,0 }; // weights for each skinning node
		};

		struct RenderModel{
			std::vector<BufferVertex> vertices; // compacted vertices for exactly what will go to the shader
			std::vector<uint32_t> indices ; // each three will be interpretted as a triangle
			Variant color_texture_data ; // ARGB format
			int texture_width = 0;
			int texture_height = 0;
		};


		struct alignas(16) Instance256{
			alignas(16) glm::mat4 root;
			alignas(16) glm::mat4 bone_pose[256]; // NOTE: this length must match the fixed value in the shader even if there are fewer bones!
		};

		struct alignas(16) Instance64 {
			alignas(16) glm::mat4 root;
			alignas(16) glm::mat4 bone_pose[64]; // NOTE: this length must match the fixed value in the shader even if there are fewer bones!
		};

		struct alignas(16) Instance1 {
			alignas(16) glm::mat4 root;
		};


		//Data structures to load VRM 1.0 spring bone extension
		struct SpringJoint{
			int node = -1;
			float radius = 0 ;
			float stiffness = 0 ;
			float drag = 0 ;
			glm::vec3 gravity ;

		};
	
		struct SpringChain{
			std::string name = "";
			std::vector<SpringJoint> joints ;
			std::vector<int> collider_groups ;
		};

		struct SphereCollider{
			int node = -1;
			glm::vec3 offset ;
			float radius  = 0;
		};

		struct CollisionGroup{
			std::string name  = "";
			std::vector<int> colliders ;
		};

        Variant json;
        Variant bin;
        std::map<int,std::map<int,int>> joint_to_node ; // joint_to_node[skin_id][joint_index] -> node_id
        std::map<int, glm::mat4> joint_inverse_bind_matrix; // maps from node id but may not be defined for all nodes
        std::vector<Node> nodes ;
        std::vector<int> root_nodes ;
        std::vector<Node> original_pose;
        glm::mat4 transform;
        std::vector<Animation> animations;
		std::vector<std::string> morph_names ;// maps index to name of morph targets
		std::vector<int> morph_triangles ; // A list of all triangles that contain morphs on any vertices (computed on demand)

        //VRM extension for avatar binding
		int VRM_version = -1 ; // -1 means non, then there are 0 and 1 versions
        std::map<std::string,int> human_bone ;
        int first_person_bone ;
        glm::vec3 first_person_offset;
        bool boneless = false; // if set to true bones will be ignored in shader (improves performance for unrigged models)

		//VR extension for Spring bones
		std::vector<SphereCollider> colliders ;
		std::vector<CollisionGroup> collision_groups ;
		std::vector<SpringChain> spring_chains ;

        std::unordered_map<std::string, Pin> pins ; // for inverse kinematics
        float barrier_strength = 0.01f;
        float stiffness_strength = 0.005f;
        float tolerance = 0.0014f ;
        float stiffness_decay = 0.75f;
        int lbfgs_m = 5;
        int iter = 20;
        int step_iter = 20;



        std::vector<Vertex> vertices ;
        std::vector<Triangle> triangles ; 
        std::map<int,Material> materials;
        std::map<int,Image> images;
        glm::vec3 min; // minimum values in each axis part of AABB
        glm::vec3 max; // maximum values in each axis part of AABB
        bool position_changed = false;
        bool model_changed = false;
        bool bones_changed = false;
        int last_traced_tri ; // Index of last triangle hit by raytrace

        int buffer_stopped_material_index = 0; // if buffer collection fails due to memory limits this is where it should pick up 
        // Constructor
        GLTF();

        //Destructor
        ~GLTF() override;
        
        
        Variant getChangedBuffer(int selected_material);

        std::map<std::string, Variant> getChangedBufferObject(int selected_material);

        Variant getBoneData();

        Variant getCompressedBoneData();

        Variant getBoneData(const Variant& compressed);

		std::vector<glm::mat4> getBoneVector() ;

		// returns the data necesarry to create an indexed Vulkan render call
		// one model for each material
		std::vector< std::shared_ptr<GLTF::RenderModel>> getRenderBuffers(bool include_morphable = false);

		//Like getRenderBuffers but returns only the morphable triangles of the model morphed with the given morph weights
		std::vector< std::shared_ptr<GLTF::RenderModel>> getMorphedRenderBuffers(std::vector<float> weights, bool include_texture = true);

		

		// Returns instance data containing a base pose andall bone poses in mat4 format
		std::shared_ptr<GLTF::Instance256> getPoseBuffer();

		//returns a transform that would center this model at 0,0,0 with roughly a size of 1
		glm::mat4 getNormalizationTransform() const;

        // Sets this Model to a chunk of raw GLB data
        void setModel(const byte* data, int data_length);

        // Compacts the given vertices and sets the model to them
        void setModel(const std::vector<Vertex>& vertices, const std::vector<Triangle>& triangles);

        void receiveTableData(std::string key, const Variant& data) override;

        // Sets the model to a single tetrahedron (can be used as a placeholder or for debugging without a model)
        void setTetraModel(glm::vec3 center, float size);

		// Sets the model to a single axis aligned bounding box (can be used as a placeholder or for debugging without a model)
		void setBoundingBoxModel(const glm::vec3& min, const glm::vec3& max, const glm::vec4 color);

        // Sets the model to a polyhedron of the given color (Can be used to generate visuals for ConvexShape objects)
        void setPolyhedronModel(const std::vector<glm::vec3>& vertices, const std::vector<std::vector<int>>& faces, glm::vec4 color);

        void addPrimitive(std::vector<Vertex>& vertices, std::vector<Triangle>& triangles,
            Variant& primitive, int node_id, const glm::mat4& transform, Variant& json, const Variant& bin);

        void addMesh(std::vector<Vertex>& vertices, std::vector<Triangle>& triangles,
            int mesh_id, int node_id, const glm::mat4& transform, Variant& json, const Variant& bin);

        void addNode(std::vector<Vertex>& vertices, std::vector<Triangle>& triangles,
            int node_id, const glm::mat4& transform, Variant& json, const Variant& bin);

        void addScene(std::vector<Vertex>& vertices, std::vector<Triangle>& triangles,
            int scene_id, Variant& json, const Variant& bin);

        static Accessor access(int accessor_id, Variant& json, const Variant& bin);

        static Accessor access(int accessor_id, std::vector<Variant>& accessors, std::vector<Variant>& views, const Variant& bin);

        void addMaterial(int material_id, Variant& json, const Variant& bin);

        bool addImage(int image_id, Variant& json, const Variant& bin);

        void addAnimation(Variant& animation, Variant& json, const Variant& bin);

		void loadVRMExtensions(Variant& json);

        // Computes absolute node matrices from their componentsand nesting
        void computeNodeMatrices(int node_id, const glm::mat4& transform);

        // calls above with all roots using the root transform
        void computeNodeMatrices();
        
        // Computes base vertices for skinned vertices so they can later use apply node transforms
        void computeInvMatrices();

        // Applies current absolute node matrices to skinned vertices
        void applyTransforms();

        // Sets active pose to the base pose
        void setToBasePose();

        //Sets the active pose and base pose to what they were whenthe file was loaded
        void setToOriginalPose();

        static glm::quat slerp(glm::quat A, glm::quat B, float t);

        // Applies a quaternion rotation to x and returns the result
        static glm::vec3 applyRotation(const glm::vec3 x, const glm::quat rot);

        // Computes the gradient of a rotation's quaternion with respect to an error given gradient of x output to that error
        static glm::vec4 dedq(const glm::vec3 x, const glm::quat rot, const glm::vec3 dedx);

        // Computes the gradient of a rotation's input with respect to an error given gradient of x output to that error
        static glm::vec3 dedx(const glm::vec3 x, const glm::quat rot, const glm::vec3 dedx);

        // hashes a vertex to allow duplicates to be detected
        int hashVertex(glm::vec3 v);

        // hashes a vertex to allow duplicates to be detected
        // rounds vertex to tolerance first
        int64_t hashVertex(const glm::vec3& v, float tolerance);

        // returns the model with {vertices:float_array vertices, faces:(int_array or short_array) triangles}
        std::map<std::string,Variant> getModel();

        // Given a ray in model space (p + v*t) return the t value of the nearest collision with the bounding box
        // return negative if no collision
        float rayTraceBoundingBox(const glm::vec3& p, const glm::vec3& v);

        // Given a ray in model space (p + v*t) return the t value of the nearest collision
        // return negative if no collision
        float rayTrace(const glm::vec3 &p, const glm::vec3 &v);

        // Returns the index of the closest vertex to the given point
        int getClosestVertex(const glm::vec3 &p);

        // Returns transforms for the given animation 
        // Uses current values for transforms unaffected by animation (with weight=0), does not apply transforms to vertices
        std::vector<GLTF::Node> getPose(Animation& animation, float time);

        // same as above but takes int index int loaded animations already on this GLTF
        std::vector<GLTF::Node> getPose(int selected_animation, float time);

        // Returns the currently active pose
        std::vector<GLTF::Node> getCurrentPose();

        //Sets the current pose to the given pose (both main and base), does not apply transforms to vertices
        void setPose(std::vector<GLTF::Node>& pose);

        //Sets the base pose on the given pose to the nonbase pose of the base_source
        static void setBasePose(std::vector<GLTF::Node>& pose , std::vector<GLTF::Node>& base_source);

        // Interpolates from pose A at t=0 to pose B at t = 1
        static std::vector<GLTF::Node> blendPose(const std::vector<GLTF::Node>& A, const std::vector<GLTF::Node>& B, float t);

        // Interpolates from pose A at t=0 to pose B at t = 1 with weights multipled by the node weights
        // Any nodes which have 0 weight total will be loaded from A
        static std::vector<GLTF::Node> blendPoseWeighted(const std::vector<GLTF::Node>& A, const std::vector<GLTF::Node>& B, float t);

        // Blends a set of poses with weights (multiplies weights on input poses as well)
        // Nodes which have no node weight will use the first pose regardless of its weight
        static std::vector<GLTF::Node> blendPoseWeighted(const std::vector<std::pair<std::vector<GLTF::Node>, float>> poses);

        // Create an IK pin to pull on the given bone local point
        void createPin(const std::string& name, int bone, glm::vec3 local_point, float weight);

        // Set the target for a given pin
        void setPinTarget(const std::string& name, glm::vec3 target);

        // Set the target for a given pin if it isn't close than target to its current location (prevents jitter)
        void setPinTarget(const std::string& name, glm::vec3 target, float tolerance);

        // Create an IK pin to pull on the given bone local point and target ortation
        // Returns starting orientation
        glm::quat createPin(const std::string& name, int bone, glm::vec3 local_point, float weight, float rot_weight);

        // Set the target for a given pin
        void setPinTarget(const std::string& name, glm::vec3 target, glm::quat rot_target);

        // Set the target for a given pin
        void setPinTarget(const std::string& name, glm::quat rot_target);

        // delete pin
        void deletePin(const std::string& name);

        // run inverse kinematics on model to bones to attemp to satisfy pin constraints
        void applyPins();

        void setStiffnessByDepth();

        void setStiffnessByDepth(int node_id, float stiffness);

        // Return the current x for this object
        std::vector<float> getX() override;

        // Set this object to a given x
        void setX(std::vector<float> x) override;

        // Returns the error to be minimized for the given input
        float error(std::vector<float> x) override;

        // Returns the gradient of error about a given input
        std::vector<float> gradient(std::vector<float> x) override;

        void fixedSpeedIK(float speed);

        void fixedSpeedRotationIK(float speed);

        glm::mat4 getNodeTransform(std::string name);

        glm::vec3 getFirstPersonPosition();


        //returns the bone index of the bone with the matching name or -1 if not found
        int getBoneIndex(std::string bone_name);

        // Adds the geometry from the given model to this model
        void addGeometry(std::shared_ptr<GLTF> new_geometry, const glm::mat4& transform);

        // Merges duplicate vertices
        void dedupeVertices(float tolerance);

		//Creates a copy of this model with normals flipped and winding order inverted
		//Such a model should appear correctly when a reflection transform is applied.
		std::shared_ptr<GLTF> createMirrorImage();


    private:
        // Performs the duplicate work for the various get vertex buffer functions
        Variant getFloatBuffer(std::vector<glm::vec3>& ptr, int material);
        Variant getFloatBuffer(std::vector<glm::vec2>& ptr, int material);
        Variant getFloatBuffer(std::vector<glm::vec4>& ptr, int material);
        Variant getFloatBuffer(std::vector<glm::ivec4>& ptr, int material);

        // returns the normal of a triangle
        glm::vec3 getNormal(Triangle t);

        // Given a ray (p + v*t) return the t value of the nearest collision
        // with the given triangle
        // returns negative if no collision
        float trace(Triangle tri, const glm::vec3 &p, const glm::vec3 &v);
 

};


//Global scope for specializing templates
template<>
inline void setPose<GLTF::Instance256>(GLTF::Instance256* instance,
	const glm::mat4& root_pose,
	const std::vector<glm::mat4>& bones) {
	instance->root = root_pose;
	for (size_t k = 0; k < bones.size() && k < std::size(instance->bone_pose); k++) {
		instance->bone_pose[k] = bones[k];
	}
	//printf("Setting pose 256\n");
}

template<>
inline void setPose<GLTF::Instance64>(GLTF::Instance64* instance,
	const glm::mat4& root_pose,
	const std::vector<glm::mat4>& bones) {
	instance->root = root_pose;
	for (size_t k = 0; k < bones.size() && k < std::size(instance->bone_pose); k++) {
		instance->bone_pose[k] = bones[k];
	}
	//printf("Setting pose 64\n");
}

template<>
inline void setPose<GLTF::Instance1>(GLTF::Instance1* instance,
	const glm::mat4& root_pose,
	const std::vector<glm::mat4>& bones) {
	instance->root = root_pose;
	if(bones.size() > 0){
		instance->root = instance->root * bones[0] ;
	}
	//printf("Setting pose 1\n");
}

#endif // #ifndef _GLTF_H_