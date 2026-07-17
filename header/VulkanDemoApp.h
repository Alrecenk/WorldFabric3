#ifndef _VULKAN_DEMO_APP_H_
#define _VULKAN_DEMO_APP_H_ 1

#include "AsyncPlugin.h"
#include "Variant.h"
#include "VulkanPlugin.h"
#include "MachineState.h"
#include "glm/glm.hpp"
#include "GLTF.h"
#include <stdio.h>
#include <cstdlib>

#include <string>
#include <map>






class VulkanDemoApp : public MachineState {

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


	static inline const std::string state_name = "vulkan_demo_state";

	//Loads models from the hard drive on construction
	VulkanDemoApp();

	void run() override;

	// Called when switching into this sate before the first time run is claled
	void enter(std::shared_ptr<MachineState> from) override;


	// Called when switching outof this state after the last time run is called
	void exit(std::shared_ptr<MachineState> to) override;


	//TOd should be moved to scvene plugin when that is updated
	static std::pair<std::vector<std::shared_ptr<TriangleModel<VulkanDemoApp::MeshPushConstants, GLTF::BufferVertex, GLTF::Instance256>>>, std::shared_ptr<GLTF>> loadGLTF(std::string file_path, VulkanPlugin* renderer, std::shared_ptr<TriangleShaderProgram> mesh_program);

private:
	glm::mat4 camera_matrix ;
	

	std::shared_ptr<TriangleShaderProgram> mesh_program;
	std::map<std::string, std::pair<std::vector<std::shared_ptr<TriangleModel<VulkanDemoApp::MeshPushConstants, GLTF::BufferVertex, GLTF::Instance256>>>, std::shared_ptr<GLTF>>> meshes;

	std::shared_ptr<ScreenShaderProgram>  postprocess_shader;
	std::shared_ptr<ScreenModel<ComputePushConstants, ComputeComponent>> post_effect;



	int fox_id ;
	int dragon_id ;
	int post_effect_id ;

	

	
	//std::shared_ptr<RenderTarget> createRenderTarget(int width, int height, VulkanPlugin* renderer);

};
#endif // #ifndef _VULKAN_DEMO_APP_H_