#include "ParticlePlugin.h"
#include "VulkanPlugin.h"
#include "OpenXRPlugin.h"
#include <algorithm> // Required for std::min

using glm::vec3;
using glm::vec4;
using glm::mat4;

ParticlePlugin::ParticlePlugin(VulkanPlugin* renderer, int max_visible, Variant& vertex_shader_file_data, Variant& frag_shader_file_data, std::unordered_set<std::shared_ptr<RenderTarget>> render_targets) {
	this->max_visible = max_visible; 
	
	
	VkShaderModule triangleVertexShader = renderer->loadShader(vertex_shader_file_data.getByteArray(), vertex_shader_file_data.getArrayLength());
	VkShaderModule triangleFragShader = renderer->loadShader(frag_shader_file_data.getByteArray(), frag_shader_file_data.getArrayLength());
	int num_textures = 0;
	particle_program = std::shared_ptr<TriangleShaderProgram>( new TriangleShaderProgram(
		renderer->device,
		triangleVertexShader,
		triangleFragShader,
		sizeof(ParticlePushConstants),
		num_textures,
		VK_CULL_MODE_NONE,
		*(render_targets.begin()),
		ALPHA_BLEND
	));
	vkDestroyShaderModule(renderer->device, triangleFragShader, nullptr);
	vkDestroyShaderModule(renderer->device, triangleVertexShader, nullptr);

	particle_model = std::shared_ptr<TriangleModel<ParticlePushConstants, ParticleVertex, ParticleInstance>>(new TriangleModel<ParticlePushConstants, ParticleVertex, ParticleInstance>(particle_program));
	particle_model->setConstantLocations(
		&particle_model->push_constants.world_matrix,
		&particle_model->push_constants.camera_position,
		&particle_model->push_constants.vertexBuffer,
		&particle_model->push_constants.instanceBuffer);


	std::vector<uint32_t> indices = { 0,2,1,1,2,3 }; // two triangles for a square
	std::vector<ParticleVertex> vertices = { {vec3(-1.0f,-1.0f,0.0f)},{vec3(1.0f,-1.0f,0.0f)},{vec3(-1.0f,1.0f,0.0f)},{vec3(1.0f,1.0f,0.0f)} };

	particle_model->setModel(vertices, indices);
	particle_model->phase = PARTICLE_RENDER_PHASE ;
	particle_model->group = PARTICLE_RENDER_GROUP ;
	particle_model->setTargets(render_targets) ;
	model_id = renderer->addRenderable(particle_model) ;
}


//Create a a particle and return its id
int ParticlePlugin::createParticle(int transform_group) {
	lock.lock();
	int particle_id = max_id;
	max_id++;
	particles[particle_id].pose = glm::mat4(0) ;
	//particles[particle_id].transform_group = transform_group;
	particle_groups[particle_id] = transform_group ;
	lock.unlock();
	return particle_id;
}

// All particles are a unit sphere at rthe origin morphed by pose (and transform group)
void ParticlePlugin::setPose(const int particle_id, const glm::mat4 pose) {
	particles[particle_id].pose = pose;
}

// All particles are a unit sphere at rthe origin morphed by pose (and transform group)
void ParticlePlugin::setPose(const int particle_id, glm::vec3 position, float size) {
	glm::mat4 pose(1.0f);
	pose = glm::translate(pose, position);
	pose = glm::scale(pose, glm::vec3(size));
	particles[particle_id].pose = pose;
}

// Sets thecolor (with alpha) of a partoicle
void ParticlePlugin::setColor(const int particle_id, const glm::vec4& color) {
	particles[particle_id].color = color;
}


//Sets the transform group of a particle
void ParticlePlugin::setTransformGroup(const int particle_id, const int transform_group) {
	particle_groups[particle_id] = transform_group;
}

// Destroy a particle by ID
void ParticlePlugin::destroyParticle(const int particle_id) {
	lock.lock();
	particles.erase(particle_id);
	particle_groups.erase(particle_id);
	lock.unlock();
}

//Set a grouptransform to be applied to all particles with that group set
void ParticlePlugin::setGroupTransform(int id, glm::mat4 pose) {
	group_transforms[id].first = pose;
}

//Sets hthe position of the viewer, used for sorting for alpha blending
void ParticlePlugin::setViewPosition(const glm::vec3& viewer){
	this->viewer = viewer ;
}

// Called on every plug-in before any plug-ins are run
void ParticlePlugin::initialize() {
	printf("Particle plugin initialized.\n");
}

void ParticlePlugin::run() {

	lock.lock();
	//Buffer the group transforms
	for (auto& [id, g] : group_transforms) {
		g.second = g.first;
	}
	lock.unlock();


	VulkanPlugin* renderer = getTool<VulkanPlugin>();
	OpenXRPlugin* xr = getTool<OpenXRPlugin>();
	if (OpenXRPlugin::ENABLED) {
		viewer = (xr->left_eye_target->camera_position + xr->right_eye_target->camera_position) * 0.5f;
	}
	else {
		viewer = renderer->window_target->camera_position;
	}

	lock.lock();
	std::map<double, std::vector<int>> dist_to_id ; // map distance to id
	
	for (auto& [id, particle] : particles) {
		
		glm::mat4 pose = particle.pose;
		if (group_transforms.find(particle_groups[id]) != group_transforms.end()) {
			pose = group_transforms[particle_groups[id]].second * pose;
		}

		vec3 C = vec3(pose[3][0],pose[3][1], pose[3][2]);
		vec3 to_particle = C - viewer;
		double distance = glm::dot(to_particle, to_particle);
		dist_to_id[distance].push_back(id);
			
			
	}
	

	int num_instances = std::min(max_visible, (int)particles.size());
	std::vector<ParticleInstance> instances = std::vector<ParticleInstance>(num_instances);
	int i = num_instances - 1;
	for (auto& [dist, ids] : dist_to_id) { // map is order from smallest to largest key by default
		for (int id : ids) {
			instances[i] = particles[id];
			// in the shader the pose is multiplied by the transform group and the transformgrup is ignored
			if (group_transforms.find(particle_groups[id]) != group_transforms.end()) {
				instances[i].pose = group_transforms[particle_groups[id]].second * instances[i].pose;
			}
			//Variant(instances[i].pose).printFormatted();
			i--;
			if(i < 0){
				break ;
			}
		}
		if (i < 0) {
			break;
		}
	}
	lock.unlock();
	particle_model->setInstances(instances);

	/*
	printf("Particle instances:\n");
	for(int k=0;k<instances.size();k++){
		vec3 p = instances[k].pose * glm::vec4(0,0,0,1);
		vec3 to_particle = p - viewer;
		double distance = glm::dot(to_particle, to_particle);
		printf("p: %d dist: %f\n", k,distance);
	}*/


}