#include "VulkanDemoApp.h"
#include "OpenXRPlugin.h"
#include "ParticlePlugin.h"
#include "FlagSet.h"

//Loads models from the hard drive on construction
VulkanDemoApp::VulkanDemoApp() {

}




// Called when switching into this sate before the first time run is claled
void VulkanDemoApp::enter(std::shared_ptr<MachineState> from) {
	VulkanPlugin* window = getTool<VulkanPlugin>();
	OpenXRPlugin* xr = getTool<OpenXRPlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	printf("entered vulkan demo app\n");

	

	// Load the shader for the mesh pipeline
	Variant vertex_shader_file_data = Variant::loadFileBytes("./shader/colored_triangle_mesh.vert.spv");
	VkShaderModule triangleVertexShader = window->loadShader(vertex_shader_file_data.getByteArray(), vertex_shader_file_data.getArrayLength());
	Variant frag_shader_file_data = Variant::loadFileBytes("./shader/colored_triangle.frag.spv");
	VkShaderModule triangleFragShader = window->loadShader(frag_shader_file_data.getByteArray(), frag_shader_file_data.getArrayLength());
	int num_textures = 1;
	mesh_program = std::shared_ptr<TriangleShaderProgram>(new TriangleShaderProgram(
		window->device, 
		triangleVertexShader, 
		triangleFragShader, 
		sizeof(VulkanDemoApp::MeshPushConstants),
		num_textures,
		VK_CULL_MODE_BACK_BIT, 
		window->window_target, 
		OVERWRITE
	));
	vkDestroyShaderModule(window->device, triangleFragShader, nullptr);
	vkDestroyShaderModule(window->device, triangleVertexShader, nullptr);

	std::unordered_set<std::shared_ptr<RenderTarget>> targets;
	targets.insert(window->window_target);
	if (OpenXRPlugin::ENABLED) {
		targets.insert(xr->left_eye_target);
		targets.insert(xr->right_eye_target);
	}

	// Scatter some foxes around
	std::string file_path = "./assets/Fox2_tail_sway.glb";
	meshes["fox"] = loadGLTF(file_path, window, mesh_program);
	meshes["fox"].second->transform = meshes["fox"].second->getNormalizationTransform();
	int num_instances = 500;
	std::vector<GLTF::Instance256> mesh_instances = std::vector<GLTF::Instance256>(num_instances);
	glm::mat4 model_pose = glm::mat4(1.0f);
	model_pose = glm::scale(model_pose, glm::vec3(0.1, 0.1, 0.1));
	model_pose = glm::rotate(model_pose, 45.0f, glm::vec3(1, 0, 0));
	float d = 40.0f;
	for (int k = 0; k < num_instances; k++) {
		mesh_instances[k].root = glm::translate(model_pose, glm::vec3((randomFloat() * d) - d / 2, (randomFloat() * d) - d / 2, (randomFloat() * d) - d / 2));
		mesh_instances[k].root = glm::rotate(mesh_instances[k].root, 90.0f, glm::vec3(1, 0, 0));
		mesh_instances[k].root = glm::rotate(mesh_instances[k].root, randomFloat() * 6.28f, glm::vec3(0.0, 1, 0));
		for(int j=0;j<50;j++){
			mesh_instances[k].bone_pose[j] = glm::mat4(1.0) ;
		}
	}
	for (int k = 0; k < meshes["fox"].first.size(); k++) {
		meshes["fox"].first[k]->setInstances(mesh_instances);
	}

	auto& fr = meshes["fox"].first[0] ;
	fr->phase = 0 ;
	fr->group = 1;
	fr->setTargets(targets);
	fox_id = window->addRenderable(fr);

	// Scatter some dragons around
	file_path = "./assets/Dragon.glb";
	meshes["dragon"] = loadGLTF(file_path, window, mesh_program);
	meshes["dragon"].second->transform = meshes["dragon"].second->getNormalizationTransform();
	num_instances = 100;
	std::vector<GLTF::Instance256> mesh_instances2 = std::vector<GLTF::Instance256>(num_instances);
	model_pose = glm::mat4(1.0f);
	model_pose = glm::scale(model_pose, glm::vec3(2.0, 2.0, 2.0));
	model_pose = glm::rotate(model_pose, 45.0f, glm::vec3(1, 0, 0));
	d = 4.0f;
	for (int k = 0; k < num_instances; k++) {
		mesh_instances2[k].root = glm::translate(model_pose, glm::vec3((randomFloat() * d) - d / 2, (randomFloat() * d) - d / 2, (randomFloat() * d) - d / 2));
		mesh_instances2[k].root = glm::rotate(mesh_instances2[k].root, 90.0f, glm::vec3(1, 0, 0));
		mesh_instances2[k].root = glm::rotate(mesh_instances2[k].root, randomFloat() * 6.28f, glm::vec3(0.0, 1, 0));
		for (int j = 0; j < 250; j++) {
			mesh_instances2[k].bone_pose[j] = glm::mat4(1.0);
		}
	}
	for (int k = 0; k < meshes["dragon"].first.size(); k++) {
		meshes["dragon"].first[k]->setInstances(mesh_instances2);
	}

	auto& dr = meshes["dragon"].first[0];
	dr->phase = 0;
	dr->group = 1;
	dr->setTargets(targets);
	dragon_id = window->addRenderable(dr);

	// Set up the postprocessor to handle lighting
	Variant gradient_shader_file_data = Variant::loadFileBytes("./shader/gradient_color.comp.spv");
	VkShaderModule gradientShader = window->loadShader(gradient_shader_file_data.getByteArray(), gradient_shader_file_data.getArrayLength());
	//std::vector< std::shared_ptr<VulkanImage>> screen_images = {window_color_image, window_normal_image, window_point_image };
	postprocess_shader = std::shared_ptr< ScreenShaderProgram>( new ScreenShaderProgram(window->device, gradientShader, sizeof(ComputePushConstants),window->window_target->images, 16));
	auto post_effect = std::shared_ptr<ScreenModel<ComputePushConstants, ComputeComponent>>(new ScreenModel<ComputePushConstants, ComputeComponent>(postprocess_shader));
	std::vector<ComputeComponent> components = { {glm::vec4(1,1,0,1)} };
	post_effect->setModel(components);
	post_effect->setConstantLocations(&post_effect->push_constants.world_matrix, &post_effect->push_constants.camera_position, &post_effect->push_constants.component_buffer);
	post_effect->phase = 1;
	post_effect->setTargets(targets);
	post_effect_id = window->addRenderable(post_effect) ;


	for(int k=0;k<2000;k++){

		glm::mat4 pose = glm::mat4(1.0);
		
		
		pose = glm::translate(pose, glm::vec3(5*(randomFloat()-.5), 5 * (randomFloat() - .5), 5 * (randomFloat() - .5))) ;
		pose = glm::scale(pose, glm::vec3(0.02, 0.02, 0.02));

		glm::vec4 color = glm::vec4(randomFloat(), randomFloat(), randomFloat(), randomFloat()) ;
		int id = particles->createParticle(0) ;
		particles->setPose(id, pose);
		particles->setColor(id, color);
	}

}


// Called when switching outof this state after the last time run is called
void VulkanDemoApp::exit(std::shared_ptr<MachineState> to) {
	
}

// Called when switching into this sate before the first time run is claled
void VulkanDemoApp::run() {

	VulkanPlugin* window = getTool<VulkanPlugin>();
	OpenXRPlugin* scene = getTool<OpenXRPlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();

	// update the camera
	glm::vec3 P = { 0,0,-3 };
	glm::vec3 Z = { 0,0,1 };
	float fov = 1.3f;
	glm::mat4 look_at = glm::lookAt(P, P + Z, glm::vec3(0, 1, 0));
	glm::mat4 projection = glm::perspective(fov, window->window_target->width / (float)window->window_target->height, 0.1f, 1000.0f);
	//glm::mat4 rotate = glm::rotate(glm::mat4(1.0f), ts, glm::vec3(0, 1, 0));
	window->window_target->camera_matrix = projection * look_at; // * rotate;


/*
	if(OpenXRPlugin::ENABLED){
		particles->setViewPosition((xr->left_eye_target->camera_position + xr->right_eye_target->camera_position)*0.5f);
	}else{
		particles->setViewPosition(P);
	}*/

	// wag some tails
	auto& gltf = meshes["fox"].second;
	auto& model_meshes = meshes["fox"].first;
	int anim_id = 0;
	float duration = gltf->animations[anim_id].duration;
	for (int i = 0; i < model_meshes[0]->instances.size(); i++) {
		float t = ((timeMilliseconds() + i * 300) % (unsigned long)(duration * 1000)) / 1000.0f;

		std::vector<GLTF::Node> bones = gltf->getPose(anim_id, t);
		gltf->setPose(bones);
		std::shared_ptr<GLTF::Instance256> inst_ptr = gltf->getPoseBuffer();
		GLTF::Instance256 inst = *inst_ptr.get();
		for (int k = 0; k < model_meshes.size(); k++) {
			inst.root = glm::rotate(model_meshes[k]->getInstance(i).root,0.01f,glm::vec3(0,1,0));
			model_meshes[k]->setInstance(i, inst);
		}
	}
	
	// flap some wings
	auto& gltf2 = meshes["dragon"].second;
	auto& model_meshes2 = meshes["dragon"].first;
	anim_id = 2;
	duration = gltf2->animations[anim_id].duration;
	for (int i = 0; i < model_meshes2[0]->instances.size(); i++) {
		float t = ((timeMilliseconds() + i * 300) % (unsigned long)(duration * 1000)) / 1000.0f;

		std::vector<GLTF::Node> bones = gltf2->getPose(anim_id, t);
		gltf2->setPose(bones);
		std::shared_ptr<GLTF::Instance256> inst_ptr = gltf2->getPoseBuffer();
		GLTF::Instance256 inst = *inst_ptr.get();
		for (int k = 0; k < model_meshes2.size(); k++) {
			inst.root = model_meshes2[k]->getInstance(i).root ;
			model_meshes2[k]->setInstance(i, inst);
		}
	}

	// Check if escape pressed to exit
	if(window->getLastKeyPress() == SDLK_ESCAPE){
		getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
	}

}


std::pair<std::vector<std::shared_ptr<TriangleModel<VulkanDemoApp::MeshPushConstants, GLTF::BufferVertex, GLTF::Instance256>>>, std::shared_ptr<GLTF>> VulkanDemoApp::loadGLTF(std::string file_path, VulkanPlugin* window, std::shared_ptr<TriangleShaderProgram> mesh_program) {


	std::vector<std::shared_ptr<TriangleModel<VulkanDemoApp::MeshPushConstants, GLTF::BufferVertex, GLTF::Instance256>>> gltf_meshes;
	std::shared_ptr<GLTF> gltf_model;

	Variant file_data = Variant::loadFileBytes(file_path);
	gltf_model = std::shared_ptr<GLTF>(new GLTF);
	gltf_model->setModel(file_data.getByteArray(), file_data.getArrayLength());

	std::vector< std::shared_ptr<GLTF::RenderModel>> render_buffers = gltf_model->getRenderBuffers();

	for (std::shared_ptr<GLTF::RenderModel>& rb : render_buffers) {
		auto mat_mesh = std::shared_ptr<TriangleModel<VulkanDemoApp::MeshPushConstants, GLTF::BufferVertex, GLTF::Instance256>>(new TriangleModel<VulkanDemoApp::MeshPushConstants, GLTF::BufferVertex, GLTF::Instance256>(mesh_program));
		mat_mesh->setConstantLocations(
			&mat_mesh->push_constants.world_matrix,
			&mat_mesh->push_constants.camera_position,
			&mat_mesh->push_constants.vertexBuffer,
			&mat_mesh->push_constants.instanceBuffer);

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
		gltf_texture->setSampler(samplerInfo) ;
		mat_mesh->setTextures({ gltf_texture });
		gltf_meshes.push_back(mat_mesh);
	}
	return { gltf_meshes,gltf_model };

}

