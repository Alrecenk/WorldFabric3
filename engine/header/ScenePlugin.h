#ifndef _SCENE_PLUGIN_H_
#define _SCENE_PLUGIN_H_ 1

#include "AsyncPlugin.h"
#include "Variant.h"
#include "VulkanPlugin.h"
#include "OpenXRPlugin.h"
#include "ConvexShape.h"

#include "Utilities.h"
#include "glm/glm.hpp"

#include <stdio.h>
#include <cstdlib>

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <unordered_map>
#include <mutex>

#include "GLTF.h"



template<typename T>
concept HasFragmentBuffer = requires(T t) { t.fragment_buffer; };

class ScenePlugin : public AsyncPlugin {

public:

	static inline std::string tag = "SceneLink";
	static inline bool IK_ENABLED = true;
	static inline bool SHADOWS_ENABLED = true;
	static inline int MAIN_PHASE = 1;
	static inline int SHADOW_PHASE = 2;
	static inline int LIGHT_PHASE = 10 ; // lights are onseparate phases so actual phase is light_phase+light_id
	//particle phase is typically 1000, so it would go here, defined in particleplugin
	static inline int TRANSLUCENT_PHASE = 2000 ;
	static inline int TRANSLUCENT_POST_PHASE = 2001;
	static inline int shadow_resolution = 1024;
	//ui phase is typically 10000

	static inline const int READY_FOR_SCENE = 56296549;//thread signal for when the scene waits to run

	struct DefaultPushConstants {
		glm::mat4 world_matrix;
		alignas(16) glm::vec3 camera_position;
		VkDeviceAddress vertexBuffer;
		VkDeviceAddress instanceBuffer;
	};

	class AbstractShaderSet {
	public:
		int max_bones = 0; // maximum bones these shader supports
		std::shared_ptr<TriangleShaderProgram> main_program;
		std::shared_ptr<TriangleShaderProgram> shadow_program;
	
		virtual std::vector<std::shared_ptr<Renderable>> loadGLTF(std::shared_ptr<GLTF> gltf_model, VulkanPlugin* renderer, std::shared_ptr<TriangleShaderProgram> mesh_program)  = 0 ;

		virtual void setMorphModels(std::shared_ptr<GLTF> gltf_model, VulkanPlugin* renderer, std::vector<float>& weights) = 0 ;
	};

	// A GLTF and all of its triangle models on the Vulkan Plugin
	class GLTFModelSet {
	public:
		std::shared_ptr<GLTF> gltf_model;
		std::vector<int> main_model_ids; // can be used with getRenderable on VulkanPlugin
		std::vector<int> shadow_model_ids; // shadow models are separate so the scene can control the render targets
		bool translucent = false ; //if a model is translucent it gets sorted and can't efficiently instance
		std::shared_ptr<AbstractShaderSet> shader_set ; // shader set used to make the triangle models for this model set
	};

	struct ScreenPushConstants {
		glm::mat4 world_matrix;
		alignas(16) glm::vec3 camera_position;
		VkDeviceAddress component_buffer;
	};


	struct TranslucentPushConstants {
		glm::mat4 world_matrix;
		alignas(16) glm::vec3 camera_position;
		VkDeviceAddress vertexBuffer;
		VkDeviceAddress instanceBuffer;
		VkDeviceAddress fragment_buffer;
		VkDeviceAddress count_buffer;
		int frame_width;
		int frame_height;
		int fragments;
	};

	struct TranslucentScreenPushConstants {
		glm::mat4 world_matrix;
		alignas(16) glm::vec3 camera_position;
		VkDeviceAddress component_buffer;
		VkDeviceAddress fragment_buffer;
		VkDeviceAddress count_buffer;
		int frame_width;
		int frame_height;
		int fragments;
	};


	struct AmbientComponent {
		glm::vec4 lightcolor;
	};

	struct LightComponent {
		glm::vec4 light_position;
		glm::vec4 light_color;
		glm::mat4 light_matrix;
	};


	class Light {
	public:
		std::vector<int> post_processors ;
		std::vector <std::shared_ptr<RenderTarget>> targets ;
		int transform_grouo = -1 ;
	};

	
	// a Grouping of main and shadow shader that can be used as a default
	template <typename PushConstants, typename VulkanInstance>
	class GLTFShaderSet : public AbstractShaderSet{
	public:
		std::map<std::shared_ptr<GLTF>,std::vector<std::shared_ptr<TriangleModel<PushConstants, GLTF::BufferVertex, VulkanInstance>>>> morph_meshes ;
		
		GLTFShaderSet(int bones, std::shared_ptr<TriangleShaderProgram> main,std::shared_ptr<TriangleShaderProgram> shadow){
			max_bones = bones;
			main_program = main ;
			shadow_program = shadow ;
		}
		
		std::vector <std::shared_ptr<Renderable>> loadGLTF(std::shared_ptr<GLTF> gltf_model, VulkanPlugin* renderer, std::shared_ptr<TriangleShaderProgram> mesh_program) override{
			std::vector<std::shared_ptr<Renderable>> gltf_meshes;
			std::vector< std::shared_ptr<GLTF::RenderModel>> render_buffers = gltf_model->getRenderBuffers();
			std::vector< std::shared_ptr<GLTF::RenderModel>> morph_render_buffers = gltf_model->getMorphedRenderBuffers(std::vector<float>());
			std::vector<int> morph_indices ;
			for(auto& m_buffer : morph_render_buffers){
				morph_indices.push_back((int)render_buffers.size()) ;
				render_buffers.push_back(m_buffer) ;
			}

			for (std::shared_ptr<GLTF::RenderModel>& rb : render_buffers) {
				auto mat_mesh = std::shared_ptr<TriangleModel<PushConstants, GLTF::BufferVertex, VulkanInstance>>(new TriangleModel<PushConstants, GLTF::BufferVertex, VulkanInstance>(mesh_program));
				mat_mesh->setConstantLocations(
					&mat_mesh->push_constants.world_matrix,
					&mat_mesh->push_constants.camera_position,
					&mat_mesh->push_constants.vertexBuffer,
					&mat_mesh->push_constants.instanceBuffer);

				if constexpr (HasFragmentBuffer<PushConstants>) {
					mat_mesh->setExtendedFragmentLocations(
						&mat_mesh->push_constants.frame_width,
						&mat_mesh->push_constants.frame_height,
						&mat_mesh->push_constants.fragments,
						&mat_mesh->push_constants.fragment_buffer,
						&mat_mesh->push_constants.count_buffer);
				}

				mat_mesh->setModel(rb->vertices, rb->indices);


				std::shared_ptr<WFImage> gltf_texture = std::shared_ptr<WFImage>(new WFImage(rb->texture_width, rb->texture_height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));

				if (rb->texture_width > 0) {
					gltf_texture->setImage(rb->color_texture_data.getByteArray(), (uint32_t)rb->texture_width, (uint32_t)rb->texture_height);
				}

				VkSamplerCreateInfo samplerInfo{};
				samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
				samplerInfo.magFilter = VK_FILTER_LINEAR;
				samplerInfo.minFilter = VK_FILTER_LINEAR;
				samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				samplerInfo.anisotropyEnable = VK_FALSE;
				samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
				samplerInfo.unnormalizedCoordinates = VK_FALSE;
				samplerInfo.compareEnable = VK_FALSE;
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				gltf_texture->setSampler(samplerInfo);

				mat_mesh->setTextures({ gltf_texture });
				gltf_meshes.push_back(mat_mesh);
			}

			for(int k : morph_indices){ // keep references to the morphable part so we can quickly access it to morph later
				morph_meshes[gltf_model].push_back(dynamic_pointer_cast<TriangleModel<PushConstants, GLTF::BufferVertex, VulkanInstance>>(gltf_meshes[k])) ;
			}

			return gltf_meshes;

		}

		void setMorphModels(std::shared_ptr<GLTF> gltf_model, VulkanPlugin* renderer, std::vector<float>& weights){
			std::vector< std::shared_ptr<GLTF::RenderModel>> morphs = gltf_model->getMorphedRenderBuffers(weights, false);
			int j = 0 ;
			//Shader set has renderables for each shader, so we need to push the morphs to all of them ever loaded
			//each one should have a number of morphs equal to the size of the morphs array
			for(auto& mesh : morph_meshes[gltf_model]){
				mesh->setModel(morphs[j%morphs.size()]->vertices, morphs[j%morphs.size()]->indices);
				j++;
			}

		}

	};

	class AnimationInstance {
	public:
		std::string animation_name = "";
		float time = 0.0f;
		float weight = 1.0f;
		float speed = 1.0f;
		bool looping = true;
		bool done = false; // whether a nonlooping animation has finished

		AnimationInstance(std::string name, bool loop) {
			animation_name = name;
			looping = loop;
		}
		AnimationInstance() {}
	};


	class BoneOverride {
	public:
		glm::vec3 scale;
		bool scale_override = false;
		glm::quat orientation;
		bool orientation_override = false;
	};

	class Capsule {
	public:
		int node = -1;
		std::pair<glm::vec3, glm::vec3> local_center; // center and radius in bone space
		float local_radius = 0;
		std::pair<glm::vec3, glm::vec3> world_center; // center and radius in world space
		float world_radius = 0;
	};

	class SpringBone {
	public:
		int node = -1;
		int depth = -1; // Springs needs to be executed in depth order
		glm::vec3 local_point; // The point in bone space this spring is controlling
		glm::vec3 world_point; // The physically simulated point in World Space
		glm::vec3 prev_world_point ; // We use verlet integration for stability so this implicitly holds the current velocity
		glm::vec3 last_target ;
		bool reset = true ;
		float half_return_time = 0.25f ; // the amoutn of time it takes a spring bone to return halfway to it's model position
		float half_velocity_time = 0.03f ; // the amount fo time it takes aspring bone point to lsoe half of its velocity
		glm::vec3 acceleration; //external force being applied
		float collision_radius ;
		std::vector<int> colliders ; // Indices into collider list of which colliders this needs to pay attention to
	};

	class Instance {
	public:
		glm::mat4 pose;
		int transform_group = 0; // this instance also gets a group transform applied to it after its own pose
		//glm::mat4 inv_pose;
		std::shared_ptr<GLTF> skeleton;
		std::vector<glm::mat4> bone_data; // The pose of the models bones if set (may be empty)
		std::unordered_map<int, int> models; // maps an internal model reference id to the scene plugin's global model set list
		std::vector<GLTFModelSet*> cached_model_sets ; // cache model sets so we don't have to find them when rendering
		
		std::unordered_map<int, AnimationInstance> animations;
		std::chrono::high_resolution_clock::time_point last_animation_update_time;

		bool IK_enabled = false;
		std::unordered_map<std::string, GLTF::Pin> pins;
		std::vector<GLTF::Node> last_pose;
		std::map<int, BoneOverride> bone_override; // overide specific bones to be in a given pose regardless of IK or animation
		std::map<int, SpringBone> spring_bones ;
		std::vector<Capsule> colliders;

		int next_animation_id = 0;
		int next_model_id = 1;
		int input_num = -1 ;

		//std::mutex lock;

		
		std::shared_ptr<void> main_instance ;
		std::shared_ptr<void> shadow_instance;


		void cacheModelSets(ScenePlugin* scene){
			cached_model_sets = std::vector<GLTFModelSet*>();
			cached_model_sets.reserve(models.size());
			for(auto& [id, model_set_id] : models){
				cached_model_sets.push_back(&scene->model_sets[model_set_id]) ;
			}
		}
	};

	

	ScenePlugin(VulkanPlugin* r, OpenXRPlugin* x);


	// Create a model set that can be attached to instances
	// This assumes the models are already loaded onto the vulkanplugin with the given ids
	void createModelSet(const std::string& name, std::shared_ptr<GLTF> gltf, std::vector<int> model_ids, std::vector<int> shadow_model_ids, bool translucent = false);

	//Convenience method to create a ModelSet from a gltf file using the default GLTF Vertex and Instance but allowing specifying custom shaders
	// Creates and adds the GLTF models tu the Vulkan Renderer
	//void createModelSet(const std::string& name, const std::string& glb_file_path, std::shared_ptr<TriangleShaderProgram> main_program, std::shared_ptr<TriangleShaderProgram> shadow_program, bool normalize);

	//Convenience method to create a ModelSet from a gltf file using the default GLTF Vertex and Instance and default shaders
	// Creates and adds the GLTF models to the Vulkan Renderer
	void createModelSet(const std::string& name, const std::string& glb_file_path, bool normalize = false, bool translucent = false);


	//Convenience method to create a ModelSet from a gltf using the default GLTF Vertex and Instance and default shaders
	// Creates and adds the GLTF models to the Vulkan Renderer
	void createModelSet(const std::string& name, std::shared_ptr<GLTF>& gltf_model, bool normalize = false, bool translucent = false) ;

	// Returns whether a model by the given name currently exists
	bool hasModel(const std::string& model_name);
	bool hasModel(const int& model_set_id);

	// Deletes a model from this scene and the renderer plugin
	void deleteModel(const std::string& model_name);

	//Create an instance of an existing model
	// Returns the id of the new instance
	int createInstance(const std::string& model_name, const glm::mat4& pose);

	//Adds a model to an instance (the skeletons must match the base of that instance)
	// Returns the index of the newly added model on the instance
	int addModelToInstance(const int instance_id, const std::string& model_name);

	// Removes a model from the given instance
	void removeModelFromInstance(const int instance_id, const std::string& model_name);

	//Sets the first model on an instance
	//Short hand for the functionality of add and remove model from instance when you always want there to be only one model
	void setInstanceModel(const int instance_id, const std::string& model_name);

	// Adds an animation and makes it referencable by name
	void addAnimation(std::string animation_name, std::shared_ptr<GLTF> model, int index);

	// Adds an animation and makes it referencable by name
	void addAnimation(const std::string& animation_name, const std::string& glb_file_path, int index);

	void matchAnimationToModel(const std::string& animation_name, const std::string& model_name);

	// Adds an animation to the given instance and returns the index of that animation on the instance
	int animateInstance(int instance_id, const std::string& animation_name, bool looping = false);

	// Adds an animation to the given instance and returns the index of that animation on the instance
	int animateInstance(int instance_id, const std::string& animation_name, bool looping, float start_time, float speed);

	// Sets the weight of animation vs other animations (IK will still override)
	void setAnimationWeight(int instance_id, int animation_instance_id, float weight);

	float getAnimationWeight(int instance_id, int animation_instance_id);

	// Sets the speed of an animation which is already playing
	void setAnimationSpeed(int instance_id, int animation_instance_id, float speed);

	float getAnimationSpeed(int instance_id, int animation_instance_id);

	// Returns whether a nonloopinganimation has completed
	bool animationDone(int instance_id, int animation_instance_id);

	//Removes an animation instance from an instance
	void clearAnimation(int instance_id, int animation_instance_id);

	//Removes an animation instance from an instance and applies a tranformation to entire instance at the same time
	void clearAnimation(int instance_id, int animation_instance_id, glm::mat4 transform);

	//Sets the world pose of an instance
	std::vector<GLTF::Node> getAnimationPose(const std::string& animation_name, float time);

	//Sets the world pose of an instance
	void setPose(const int instance_id, const glm::mat4& pose);

	//Sets the world pose and bone poses of an instance
	void setPose(const int instance_id, const glm::mat4& pose, const std::vector<glm::mat4>& bone_data);

	// Returns the bone data of the given instance on the last frame
	std::vector<glm::mat4> getBoneData(const int instance_id);

	// Overrides the orientation of a specific bone on a specific instance
	void overrideBoneOrientation(const int instance_id, int bone, glm::quat orientation);

	void disableOverrideBoneOrientation(const int instance_id, int bone) ;

	// Overrides the scale of a specific bone on a specific instance
	void overrideBoneScale(const int instance_id, int bone, glm::vec3 scale);

	//gets the world pose of an instance
	glm::mat4 getPose(const int instance_id);

	//delete an Instance
	void deleteInstance(const int instance_id);

	//Used for tracking input latency to visual display
	void setInputNum(const int instance_id, const int input_num);

	// returns a pointer the the GLTF for the given model (Can be used for IK, raytracing, or physics collisions)
	std::shared_ptr<GLTF> getModelController(const std::string& model_name);

	std::shared_ptr<GLTF> getModelController(int instance_id, int model_index = 0);

	std::shared_ptr<GLTF> getAnimationController(const std::string& animation_name);

	//Fetch an approximate bounding shape of the given model (useful for cursor triggers and physics objects)
	std::shared_ptr<ConvexShape> getBoundingShape(const std::string& model_name);

	// Returns whether an instance by the given name currently exists
	bool hasInstance(const int instance_id);

	int modelCount(const int instance_id);

	int size();

	// Called on every plug-in before any plug-ins are run
	void initialize() override;

	void run() override;

	// Creates an IK pin on the given instance
	// Returns starting orientation
	glm::quat createPin(const int instance_id, const std::string& pin_name, int bone, glm::vec3 local_point, float weight, float rot_weight);

	// Set the target for a given pin i nworld coordinates
	void setPinTarget(const int instance_id, const std::string& pin_name, glm::vec3 target);

	// Set the rotation target for a given pin
	void setPinTarget(const int instance_id, const std::string& pin_name, glm::quat rot_target);

	// Sets the position and rotation target to the best match for the given matrixc
	void setPinTarget(const int instance_id, const std::string& pin_name, glm::mat4 target);

	void deletePin(const int instance_id, const std::string& pin_name);

	// Returns whether the given pin exists
	bool hasPin(const int instance_id, const std::string& pin_name);

	void enableIK(const int instance_id, bool ik_enabled);

	// Returns a pin's position in world coordinates
	glm::vec3 getPinPosition(const int instance_id, const std::string& pin_name);

	//Set a grouptrasnform to be applied to all instances with that group set
	void setGroupTransform(int id, glm::mat4 pose);

	//Associate an instance with a group transform to be applied n rendering
	void setTransformGroup(int instance_id, int group);


	//Load any spring bone data from the skeleton of the given instance
	// and enable it on that instance
	void enableVRMSpringBones(int instance_id, float gravity_strength =10.0f , float collider_scale = 0.98f, float min_spring_collision_radius = 0.03f);

	// Remove any active spring bones on the given instance
	void clearSpringBones(int instance_id) ;

	//Gets hte current world positions of a spring set on an instance
	//Mostly for visualizing for debugging
	std::vector<std::pair<glm::vec3, float>> getSpringBoneWorldPositions(int instance_id) ;

	std::vector<glm::vec3> getSpringBoneTargetPositions(int instance_id);

	std::vector<Capsule> getSpringBoneColliders(int instance_id);
	
	//Simulatyes rping bones on a ninstance that has them
	// The instance should already have had all other posing performaned on its sekelton before this is called
	void simulateSpringBones(Instance& instance, glm::mat4& instance_pose, float dt, float prev_dt);

	// Returns the vector to move A so it does not collide with B in world coordinates
	//Returns (0,0,0) if they are already not colliding
	std::pair<bool, glm::vec3> resolveCollision(const Capsule& A,const Capsule& B) ;

	//Returns the vector to move a sphere so it does not collide with B in world coordinates
	glm::vec3 resolveCollision(const glm::vec3 center, const float& radius, const Capsule& B);

	template <typename PushConstants, typename VulkanInstance>
	void addDefaultShader(std::shared_ptr<TriangleShaderProgram> main_shader, std::shared_ptr<TriangleShaderProgram> shadow_shader, int max_bones, bool translucent = false){
		if(translucent){
			default_translucent_shaders.push_back(std::shared_ptr<GLTFShaderSet<PushConstants, VulkanInstance>>(new GLTFShaderSet<PushConstants, VulkanInstance>(max_bones, main_shader, shadow_shader)));
		}else{
			default_triangle_shaders.push_back(std::shared_ptr<GLTFShaderSet<PushConstants, VulkanInstance>>( new GLTFShaderSet<PushConstants, VulkanInstance>( max_bones, main_shader, shadow_shader))) ;
		}
	}


	void setLightProgram(VkShaderModule light_shader){
		std::vector<std::shared_ptr<WFImage>> light_images;
		for (int k = 0; k < renderer->window_target->images.size(); k++) {
			light_images.push_back(renderer->window_target->images[k]);
		}
		//create an example image for the descreiptor format for the shader
		VkImageUsageFlags drawImageUsages = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		std::shared_ptr<WFImage> output_depth_image = std::shared_ptr<WFImage>(new WFImage(128, 128, VK_FORMAT_R32_SFLOAT, drawImageUsages));
		light_images.push_back(output_depth_image); // Shader needs an example of the image layout to be built

		light_program = std::shared_ptr<ScreenShaderProgram>(new ScreenShaderProgram(renderer->device, light_shader, sizeof(ScenePlugin::ScreenPushConstants), light_images, 16));
	}

	//Creates a shadow casting light
	template <typename PushConstants, typename ComputeComponent>
	int createLight(const glm::vec3& position, const glm::vec3& look_at, const glm::vec3& up, float fov, float far, int resolution, int transform_group, ComputeComponent first_component){
		int id = next_light ;
		next_light++;
		Light& light = lights[id] ;
		


		// Create a render target for the shadow map
		VkImageUsageFlags drawImageUsages = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		std::shared_ptr<WFImage> output_depth_image = std::shared_ptr<WFImage>(new WFImage(resolution, resolution, VK_FORMAT_R32_SFLOAT, drawImageUsages));
		std::shared_ptr<WFImage> depth_image = std::shared_ptr<WFImage>(new WFImage(resolution, resolution, 
			VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
		auto shadow_target = std::shared_ptr<RenderTarget>(new RenderTarget());
		VkClearColorValue background_depth = { 1.0f,1.0f,1.0f,1.0f };
		shadow_target->setImages({ output_depth_image }, { background_depth }, depth_image, output_depth_image);
		shadow_target->setCamera(position, look_at, fov,up, far);
		renderer->addRenderTarget(shadow_target);


		auto light_post_effect = std::shared_ptr<ScreenModel<PushConstants, ComputeComponent>>(new ScreenModel<PushConstants, ComputeComponent>(light_program));
		//std::vector<ScenePlugin::LightComponent> light_components = { {glm::vec4(shadow_target->camera_position,1.0f),glm::vec4(light_color,1), shadow_target->camera_matrix } };

		first_component.light_position = glm::vec4(shadow_target->camera_position, 1.0f) ;
		first_component.light_matrix = shadow_target->camera_matrix ;

		light_post_effect->setModel({first_component});
		light_post_effect->setConstantLocations(&light_post_effect->push_constants.world_matrix, &light_post_effect->push_constants.camera_position, &light_post_effect->push_constants.component_buffer);
		light_post_effect->phase = ScenePlugin::LIGHT_PHASE+id ;
		light_post_effect->group = 1;
		std::unordered_set<std::shared_ptr<RenderTarget>> light_post_effect_targets ;
		light_post_effect_targets.insert(renderer->window_target);
		if (OpenXRPlugin::ENABLED) {
			light_post_effect_targets.insert(xr->left_eye_target);
			light_post_effect_targets.insert(xr->right_eye_target);
		}
		
		light_post_effect->setTargets(light_post_effect_targets) ;

		light_post_effect->setExtraImages(shadow_target->images); // make the light map always available after the render target images on this model
		int light_effect_id = renderer->addRenderable(light_post_effect);


		light.post_processors.push_back(light_effect_id) ;
		light.targets.push_back(shadow_target) ;
		light.transform_grouo = transform_group ;
		return id ;
	}

	void destroyLight(int light_id){
	//TODO

	}

	template <typename PushConstants, typename ComputeComponent>
	void moveLight(int light_id, const glm::vec3& position, const glm::vec3& look_at, const glm::vec3& up, float fov, float far){
		Light& light = lights[light_id];
		light.targets[0]->setCamera(position, look_at, fov, up, far);
		auto light_post_effect = std::static_pointer_cast<ScreenModel<PushConstants, ComputeComponent>>( renderer->getRenderable(light.post_processors[0]));
		ComputeComponent& first_component = light_post_effect->components[0] ;
		first_component.light_position = glm::vec4(light.targets[0]->camera_position, 1.0f);
		first_component.light_matrix = light.targets[0]->camera_matrix;
		light_post_effect->model_changed =true ;
	}

	template <typename PushConstants, typename ComputeComponent>
	void setLightComponent(int light_id, const ComputeComponent& component) {
		Light& light = lights[light_id];
		auto light_post_effect = std::static_pointer_cast<ScreenModel<PushConstants, ComputeComponent>>(renderer->getRenderable(light.post_processors[0]));
		light_post_effect->components[0] = component ;
		light_post_effect->model_changed = true;
	}

	template <typename PushConstants, typename ComputeComponent>
	ComputeComponent getLightComponent(int light_id) {
		Light& light = lights[light_id];
		auto light_post_effect = std::static_pointer_cast<ScreenModel<PushConstants, ComputeComponent>>(renderer->getRenderable(light.post_processors[0]));
		return light_post_effect->components[0] ;
	}

	std::shared_ptr<RenderTarget> getAShadowTarget(){
		return lights.begin()->second.targets[0] ;
	}
	
	// if wait for signal is enabled then the scene waits for signalReady to be called before processing models
	//This hurts performance but can reduce responsiveness by guaranteeding models update on the same frame as user input
	void enableWaitForSignal(bool enable) ;

	//Tell the scene to begin processing if waitForSignal is enabled
	void signalReady();
	void signalNotReady();


	//Sets the morph weights on a model to be applied to all instances
	void setMorphWeights(std::string model_name, std::vector<float> weights);

private:
	std::unordered_map<std::string, int> name_to_model_set ;
	std::unordered_map <int, GLTFModelSet> model_sets; // Loaded GLTF files (one GLTF file may generate a bunch of triangle models)
	int next_model_id = 1 ;
	std::unordered_map <std::string, std::pair<std::shared_ptr<GLTF>, int>> animations; // maps a name of animation to the GLTF and index (animations can be in different files than the models they are applied to)
	std::unordered_map <int, Instance> instances; // Current instances of models in the scene
	int max_id = 0; // maximum id used fo an instance so far

	std::unordered_map <std::string, std::shared_ptr<GLTF>> loaded_glb_file_cache; // key is file path
	std::unordered_map <int, std::pair<glm::mat4, glm::mat4>> group_transforms; // used to manipulate objcts in groups, saved as a pair so it can buffer and be modified during scene processing

	VulkanPlugin* renderer ;// keep a reference around,so it doesn't have to be passed for everythibng
	OpenXRPlugin* xr ; // we also need XR if it ecists so we can add light shaders to the VR render targets

	std::vector<std::shared_ptr<AbstractShaderSet>> default_triangle_shaders ; // we keep multiple default shaders to efficiently support different amounts of bones
	std::vector<std::shared_ptr<AbstractShaderSet>> default_translucent_shaders; // separate shaders for objects marked translucent as they have a separate pipeline
	
	int next_custom_group = 1000000;//Models with custom shaders each get a unique group by default, those start counting from this number

	std::shared_ptr<ScreenShaderProgram> light_program ;
	std::unordered_map<int, Light> lights;
	
	int next_light = 0 ;
	bool wait_enabled = false;

	std::chrono::high_resolution_clock::time_point last_scene_run_time = now();
	float last_dt = 1.0f/120.0f;
};
#endif // #ifndef _SCENE_PLUGIN_H_