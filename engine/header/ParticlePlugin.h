#ifndef _PARTICLE_PLUGIN_H_
#define _PARTICLE_PLUGIN_H_ 1

#include "AsyncPlugin.h"
#include "Variant.h"
#include "VulkanPlugin.h"

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



struct alignas(16) ParticleVertex{
	alignas(16) glm::vec3 position ;
};

struct alignas(16) ParticleInstance{
	alignas(16) glm::mat4 pose; // pose of the particle ellipse
	alignas(16) glm::vec4 color;
};

struct alignas(16) ParticlePushConstants {
	alignas(16) glm::mat4 world_matrix;
	alignas(16) glm::vec3 camera_position;
	VkDeviceAddress vertexBuffer;
	VkDeviceAddress instanceBuffer;
};

class ParticlePlugin : public AsyncPlugin {

public:

	static inline std::string tag = "Particles";
	static inline int PARTICLE_RENDER_PHASE = 1000; // Note these are just defaults and may need to be set
	static inline int PARTICLE_RENDER_GROUP = -1; 

	ParticlePlugin(VulkanPlugin* renderer, int max_visible, Variant& vertex_shader_file_data, Variant& frag_shader_file_data, std::unordered_set<std::shared_ptr<RenderTarget>> render_targets);


	//Create a hidden particle and return its id
	int createParticle(int transform_group);

	//Create a particle and return its id
	int createParticle(int transform_group, const glm::mat4& pose, const glm::vec4& color);

	// All particles are a unit sphere at the origin morphed by pose (and transform group)
	void setPose(const int particle_id, const glm::mat4 pose);

	// A simplified setPose that makes the particle a sphere of the given size at the given position
	void setPose(const int particle_id, glm::vec3 position, float size) ;

	// Sets the color (with alpha) of a particle
	void setColor(const int particle_id, const glm::vec4& color);

	//Sets the transform group of a particle
	void setTransformGroup(const int particle_id, const int transform_group);

	// Destroy a particle by ID
	void destroyParticle(const int particle_id);

	//Set a grouptransform to be applied to all particles with that group set
	void setGroupTransform(int id, glm::mat4 pose);

	//Sets the position of the viewer, used for sorting for alpha blending
	void setViewPosition(const glm::vec3& viewer);

	// Called on every plug-in before any plug-ins are run
	void initialize() override;

	void run() override;

private:
	std::shared_ptr<TriangleShaderProgram> particle_program; // shader progra mfor particles
	std::shared_ptr<TriangleModel<ParticlePushConstants, ParticleVertex, ParticleInstance>> particle_model;
	int model_id ;
	std::unordered_map <int, ParticleInstance> particles;
	std::unordered_map<int, int> particle_groups;
	int max_visible = 0 ; // maximum number of particles visible at a time (closest shown first)
	int max_id = 0; // maximum id used so far
	glm::vec3 viewer = glm::vec3(0, 0, 0); // viewer postion is used for sorting

	// Used to buffer transforms to prevent tearing when moving a group object
	std::unordered_map <int, std::pair<glm::mat4, glm::mat4>> group_transforms;
	

};
#endif // #ifndef _PARTICLE_PLUGIN_H_