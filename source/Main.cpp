#include "AsyncPlugin.h"
#include "FlagSet.h"
#include "OpenXRPlugin.h"
#include "ScenePlugin.h"
#include "AudioPlugin.h"
#include "ParticlePlugin.h"
#include "SteamworksPlugin.h"
#include "GLTF.h"
#include "SavePlugin.h"

#include "StatePlugin.h"

#include "BallTestApp.h"
#include "VulkanDemoApp.h"
#include "SceneDemoApp.h"
#include "SceneDemoApp2.h"
#include "WorldPlugin.h"
#include "SocketTest.h"
#include "BallThrowApp.h"
#include "MirrorApp.h"
#include "TraceApp.h"
#include "ConstraintTestApp.h"
#include "CollisionTestApp.h"
#include "PyramidApp.h"

#include "ChessApp.h"

#include "Timeline.h"
#include "VulkanPlugin.h"
#include "ThreadSignals.h"
#include "PanelPlugin.h"
#include "CSVLog.h"

#include "NarballMain.h"

#include <vector>
#include <set>
#include <memory>
#include <string>
#include <chrono>
#include <thread>

using std::string;



std::shared_ptr<RenderTarget> createRenderTarget(int width, int height, VulkanPlugin* window) {
	VkClearColorValue background_color = { 0.7f,0.7f,0.9f,1.0f };
	VkClearColorValue background_normal = { 0.0f,0.0f,0.0f,0.0f };
	VkClearColorValue background_point = { 0.0f,0.0f,0.0f,0.0f };
	VkClearColorValue start_light = { 0.0f,0.0f,0.0f,0.0f };
	VkClearColorValue panel_background = { 0.0f,0.0f,0.0f,0.0f };
	//Initialize the images we will draw into
	VkImageUsageFlags drawImageUsages = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	std::shared_ptr<WFImage> color_image = std::shared_ptr<WFImage>(new WFImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, drawImageUsages));
	std::shared_ptr<WFImage> normal_image = std::shared_ptr<WFImage>(new WFImage(width, height, VK_FORMAT_R16G16B16A16_SFLOAT, drawImageUsages));
	std::shared_ptr<WFImage> point_image = std::shared_ptr<WFImage>(new WFImage(width, height, VK_FORMAT_R16G16B16A16_SFLOAT, drawImageUsages));
	std::shared_ptr<WFImage> final_image = std::shared_ptr<WFImage>(new WFImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, drawImageUsages));
	std::shared_ptr<WFImage> panel_image = std::shared_ptr<WFImage>(new WFImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, drawImageUsages));

	std::shared_ptr<WFImage> depth_image = std::shared_ptr<WFImage>(new WFImage(width, height, VK_FORMAT_D32_SFLOAT, 
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));

	std::shared_ptr<RenderTarget> target = std::shared_ptr<RenderTarget>(new RenderTarget());

	target->setImages({ color_image, normal_image, point_image,final_image, panel_image }, { background_color, background_normal, background_point, start_light, panel_background }, depth_image, final_image);
	target->createExtendedFragments(4,8,window);
	target->enableScreenResize(true) ;
	return target;
}



std::pair< std::shared_ptr<TriangleShaderProgram>, std::shared_ptr<TriangleShaderProgram>> loadSceneShader(ScenePlugin* scene, VulkanPlugin* window, const std::string& vert_main, const std::string& frag_main, const std::string& vert_shadow, const std::string& frag_shadow){

	// Load the shader for the mesh pipeline
	Variant vertex_shader_file_data = Variant::loadFileBytes(vert_main);
	VkShaderModule triangleVertexShader = window->loadShader(vertex_shader_file_data.getByteArray(), vertex_shader_file_data.getArrayLength());
	Variant frag_shader_file_data = Variant::loadFileBytes(frag_main);
	VkShaderModule triangleFragShader = window->loadShader(frag_shader_file_data.getByteArray(), frag_shader_file_data.getArrayLength());
	int num_textures = 1;
	auto mesh_program = std::shared_ptr<TriangleShaderProgram>(new TriangleShaderProgram(
		window->device,
		triangleVertexShader,
		triangleFragShader,
		sizeof(ScenePlugin::DefaultPushConstants),
		num_textures,
		VK_CULL_MODE_FRONT_BIT,
		window->window_target,
		OVERWRITE
	));
	vkDestroyShaderModule(window->device, triangleFragShader, nullptr);
	vkDestroyShaderModule(window->device, triangleVertexShader, nullptr);

	
	// Load the shader for shadow maps
	Variant shadow_vert_file_data = Variant::loadFileBytes(vert_shadow);
	VkShaderModule shadow_vertex_shader = window->loadShader(shadow_vert_file_data.getByteArray(), shadow_vert_file_data.getArrayLength());
	Variant shadow_frag_file_data = Variant::loadFileBytes(frag_shadow);
	VkShaderModule shadow_frag_shader = window->loadShader(shadow_frag_file_data.getByteArray(), shadow_frag_file_data.getArrayLength());
	auto shadow_program = std::shared_ptr<TriangleShaderProgram>(new TriangleShaderProgram(
		window->device,
		shadow_vertex_shader,
		shadow_frag_shader,
		sizeof(ScenePlugin::DefaultPushConstants),
		num_textures,
		VK_CULL_MODE_FRONT_BIT,
		scene->getAShadowTarget(),
		OVERWRITE 
	));
	vkDestroyShaderModule(window->device, shadow_vertex_shader, nullptr);
	vkDestroyShaderModule(window->device, shadow_frag_shader, nullptr);


	return {mesh_program, shadow_program};

}

std::pair< std::shared_ptr<TriangleShaderProgram>, std::shared_ptr<TriangleShaderProgram>> loadTranslucentSceneShader(ScenePlugin* scene, VulkanPlugin* window, const std::string& vert_main, const std::string& frag_main, const std::string& vert_shadow, const std::string& frag_shadow) {

	// Load the shader for the mesh pipeline
	Variant vertex_shader_file_data = Variant::loadFileBytes(vert_main);
	VkShaderModule triangleVertexShader = window->loadShader(vertex_shader_file_data.getByteArray(), vertex_shader_file_data.getArrayLength());
	Variant frag_shader_file_data = Variant::loadFileBytes(frag_main);
	VkShaderModule triangleFragShader = window->loadShader(frag_shader_file_data.getByteArray(), frag_shader_file_data.getArrayLength());
	int num_textures = 1;
	auto mesh_program = std::shared_ptr<TriangleShaderProgram>(new TriangleShaderProgram(
		window->device,
		triangleVertexShader,
		triangleFragShader,
		sizeof(ScenePlugin::TranslucentPushConstants),
		num_textures,
		VK_CULL_MODE_FRONT_BIT,
		window->window_target,
		{ } // writes to extended fragments, so no output images
	));
	vkDestroyShaderModule(window->device, triangleFragShader, nullptr);
	vkDestroyShaderModule(window->device, triangleVertexShader, nullptr);


	// Load the shader for shadow maps
	Variant shadow_vert_file_data = Variant::loadFileBytes(vert_shadow);
	VkShaderModule shadow_vertex_shader = window->loadShader(shadow_vert_file_data.getByteArray(), shadow_vert_file_data.getArrayLength());
	Variant shadow_frag_file_data = Variant::loadFileBytes(frag_shadow);
	VkShaderModule shadow_frag_shader = window->loadShader(shadow_frag_file_data.getByteArray(), shadow_frag_file_data.getArrayLength());
	auto shadow_program = std::shared_ptr<TriangleShaderProgram>(new TriangleShaderProgram(
		window->device,
		shadow_vertex_shader,
		shadow_frag_shader,
		sizeof(ScenePlugin::DefaultPushConstants),
		num_textures,
		VK_CULL_MODE_FRONT_BIT,
		scene->getAShadowTarget(),
		OVERWRITE
	));
	vkDestroyShaderModule(window->device, shadow_vertex_shader, nullptr);
	vkDestroyShaderModule(window->device, shadow_frag_shader, nullptr);

	return { mesh_program, shadow_program };

}

ScenePlugin* setUpScene(VulkanPlugin* window, OpenXRPlugin* xr){

	ScenePlugin*  scene = new ScenePlugin(window, xr);

	Variant light_shader_file_data  = Variant::loadFileBytes("./shader/LightMapPost.comp.spv");
	
	
	//light_shader_file_data.printFormatted();
	VkShaderModule light_shader = window->loadShader(light_shader_file_data.getByteArray(), light_shader_file_data.getArrayLength());
	//std::vector< std::shared_ptr<VulkanImage>> screen_images = {window_color_image, window_normal_image, window_point_image };

	scene->setLightProgram(light_shader);
	vkDestroyShaderModule(window->device, light_shader, nullptr);
	ScenePlugin::LightComponent lc ;
	
	
	lc.light_color = glm::vec4(0.01, 0.01, 0.01, 1);
	scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(glm::vec3(-5, 15, -5), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0), 0.55f, 30, 1024, 0, lc);
	
	auto shaders1 = loadSceneShader(scene, window, "./shader/GLTF1.vert.spv", "./shader/GLTF.frag.spv", "./shader/GLTFShadow1.vert.spv", "./shader/GLTFShadow.frag.spv");
	scene->addDefaultShader<ScenePlugin::DefaultPushConstants, GLTF::Instance1>(shaders1.first, shaders1.second, 1);

	auto shaders64 = loadSceneShader(scene, window, "./shader/GLTF64.vert.spv", "./shader/GLTF.frag.spv", "./shader/GLTFShadow64.vert.spv", "./shader/GLTFShadow.frag.spv");
	scene->addDefaultShader<ScenePlugin::DefaultPushConstants, GLTF::Instance64>(shaders64.first, shaders64.second, 64);

	auto shaders256 = loadSceneShader(scene, window, "./shader/GLTF256.vert.spv", "./shader/GLTF.frag.spv", "./shader/GLTFShadow256.vert.spv","./shader/GLTFShadow.frag.spv") ;
	scene->addDefaultShader<ScenePlugin::DefaultPushConstants, GLTF::Instance256>(shaders256.first, shaders256.second, 256);
	
	auto shadersblend1 = loadTranslucentSceneShader(scene, window, "./shader/GLTF1Blend.vert.spv", "./shader/GLTFBlend.frag.spv", "./shader/GLTFShadowBlend1.vert.spv", "./shader/GLTFShadowBlend.frag.spv");
	scene->addDefaultShader<ScenePlugin::TranslucentPushConstants, GLTF::Instance1>(shadersblend1.first, shadersblend1.second, 1, true);

	std::unordered_set<std::shared_ptr<RenderTarget>> targets ;
	targets.insert(window->window_target);
	if (OpenXRPlugin::ENABLED) {
		targets.insert(xr->left_eye_target);
		targets.insert(xr->right_eye_target);
	}

	// Set up the postprocessor to handle ambient lighting
	Variant ambient_shader_file_data = Variant::loadFileBytes("./shader/AmbientLightPost.comp.spv");
	VkShaderModule ambient_shader = window->loadShader(ambient_shader_file_data.getByteArray(), ambient_shader_file_data.getArrayLength());
	//std::vector< std::shared_ptr<VulkanImage>> screen_images = {window_color_image, window_normal_image, window_point_image };
	auto ambient_program = std::shared_ptr<ScreenShaderProgram>(new ScreenShaderProgram(window->device, ambient_shader, sizeof(ScenePlugin::ScreenPushConstants), window->window_target->images, 16));
	auto ambient_post_effect = std::shared_ptr<ScreenModel<ScenePlugin::ScreenPushConstants, ScenePlugin::AmbientComponent>>(new ScreenModel<ScenePlugin::ScreenPushConstants, ScenePlugin::AmbientComponent>(ambient_program));
	std::vector<ScenePlugin::AmbientComponent> ambient_components = { {glm::vec4(0.15,0.15,0.15,1)} };
	ambient_post_effect->setModel(ambient_components);
	ambient_post_effect->setConstantLocations(&ambient_post_effect->push_constants.world_matrix, &ambient_post_effect->push_constants.camera_position, &ambient_post_effect->push_constants.component_buffer);
	ambient_post_effect->phase = ScenePlugin::LIGHT_PHASE;
	ambient_post_effect->group = 0;
	
	ambient_post_effect->setTargets(targets);

	int ambient_effect_id = window->addRenderable(ambient_post_effect);
	vkDestroyShaderModule(window->device, ambient_shader, nullptr);
	

	Variant translucent_shader_file_data = Variant::loadFileBytes("./shader/TranslucentOverlay.comp.spv");
	//light_shader_file_data.printFormatted();
	VkShaderModule translucent_shader = window->loadShader(translucent_shader_file_data.getByteArray(), translucent_shader_file_data.getArrayLength());
	//std::vector< std::shared_ptr<VulkanImage>> screen_images = {window_color_image, window_normal_image, window_point_image };
	//std::vector<std::shared_ptr<VulkanImage>> overlay_images = {window->window_target->final_image} ; // color and final image
	

	auto translucent_program = std::shared_ptr<ScreenShaderProgram>(new ScreenShaderProgram(window->device, translucent_shader, sizeof(ScenePlugin::TranslucentScreenPushConstants), window->window_target->images, 16));
	auto translucent_post_effect = std::shared_ptr<ScreenModel<ScenePlugin::TranslucentScreenPushConstants, ScenePlugin::AmbientComponent>>(new ScreenModel<ScenePlugin::TranslucentScreenPushConstants, ScenePlugin::AmbientComponent>(translucent_program));
	
	translucent_post_effect->setModel(ambient_components);
	translucent_post_effect->setConstantLocations(&translucent_post_effect->push_constants.world_matrix, &translucent_post_effect->push_constants.camera_position, &translucent_post_effect->push_constants.component_buffer);
	translucent_post_effect->setExtendedFragmentLocations(
		&translucent_post_effect->push_constants.frame_width,
		&translucent_post_effect->push_constants.frame_height,
		&translucent_post_effect->push_constants.fragments,
		&translucent_post_effect->push_constants.fragment_buffer,
		&translucent_post_effect->push_constants.count_buffer
	);

	translucent_post_effect->phase = ScenePlugin::TRANSLUCENT_POST_PHASE ;
	translucent_post_effect->group = 1;
	translucent_post_effect->setTargets(targets);
	
	int translucent_effect_id = window->addRenderable(translucent_post_effect);
	vkDestroyShaderModule(window->device, translucent_shader, nullptr);

	
	
	return scene ;
}

PanelPlugin* setUpPanels(VulkanPlugin* window){
	PanelPlugin* panels = new PanelPlugin(window);


	//Crate an example target that has the image layout of a panel to giveot the element shader
	VkImageUsageFlags drawImageUsages = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	std::shared_ptr<WFImage> color_image = std::shared_ptr<WFImage>(new WFImage(16, 16, VK_FORMAT_R8G8B8A8_UNORM, drawImageUsages));
	std::shared_ptr<WFImage> depth_image = std::shared_ptr<WFImage>(new WFImage(16, 16, VK_FORMAT_D32_SFLOAT, 
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
	std::shared_ptr<RenderTarget> example_target = std::shared_ptr<RenderTarget>(new RenderTarget());
	example_target->setImages({ color_image }, { {0,0,0,1}}, depth_image, color_image);

	// Load the shader for drawing elements into panels
	Variant vertex_shader_file_data2 = Variant::loadFileBytes("./shader/PanelElement.vert.spv");
	VkShaderModule triangleVertexShader2 = window->loadShader(vertex_shader_file_data2.getByteArray(), vertex_shader_file_data2.getArrayLength());
	Variant frag_shader_file_data2 = Variant::loadFileBytes("./shader/PanelElement.frag.spv");
	VkShaderModule triangleFragShader2 = window->loadShader(frag_shader_file_data2.getByteArray(), frag_shader_file_data2.getArrayLength());
	int num_textures = 1;
	auto element_program = std::shared_ptr<TriangleShaderProgram>(new TriangleShaderProgram(
		window->device,
		triangleVertexShader2,
		triangleFragShader2,
		sizeof(PanelPlugin::DefaultPushConstants),
		num_textures,
		VK_CULL_MODE_NONE,
		example_target,
		ALPHA_BLEND
	));
	vkDestroyShaderModule(window->device, triangleFragShader2, nullptr);
	vkDestroyShaderModule(window->device, triangleVertexShader2, nullptr);

	// Load the shader for drawing the panels in 3D
	Variant vertex_shader_file_data = Variant::loadFileBytes("./shader/Panel.vert.spv");
	VkShaderModule triangleVertexShader = window->loadShader(vertex_shader_file_data.getByteArray(), vertex_shader_file_data.getArrayLength());
	Variant frag_shader_file_data = Variant::loadFileBytes("./shader/Panel.frag.spv");
	VkShaderModule triangleFragShader = window->loadShader(frag_shader_file_data.getByteArray(), frag_shader_file_data.getArrayLength());
	num_textures = 1;
	auto panel_program = std::shared_ptr<TriangleShaderProgram>(new TriangleShaderProgram(
		window->device,
		triangleVertexShader,
		triangleFragShader,
		sizeof(PanelPlugin::DefaultPushConstants),
		num_textures,
		VK_CULL_MODE_NONE,
		window->window_target,
		ALPHA_BLEND
	));
	vkDestroyShaderModule(window->device, triangleFragShader, nullptr);
	vkDestroyShaderModule(window->device, triangleVertexShader, nullptr);

	
	//load the shader for composing the drawn panels over the final screen image
	Variant screen_file_data = Variant::loadFileBytes("./shader/PanelPost.comp.spv");
	VkShaderModule computeShader = window->loadShader(screen_file_data.getByteArray(), screen_file_data.getArrayLength());
	//std::vector< std::shared_ptr<VulkanImage>> screen_images = {window_color_image, window_normal_image, window_point_image };
	std::shared_ptr<ScreenShaderProgram> screen_program = std::shared_ptr<ScreenShaderProgram>(new ScreenShaderProgram(window->device, computeShader, sizeof(PanelPlugin::ScreenPushConstants), window->window_target->images, 16));

	vkDestroyShaderModule(window->device, computeShader, nullptr);

	PanelPlugin::DefaultInstance first_post_component ;
	panels->setShaders<PanelPlugin::DefaultPushConstants, PanelPlugin::DefaultInstance, PanelPlugin::DefaultPushConstants, PanelPlugin::DefaultInstance, PanelPlugin::ScreenPushConstants, PanelPlugin::DefaultInstance>(
	element_program, panel_program, screen_program, first_post_component) ;

	std::string arial = "arial";
	panels->addFont(arial, "./assets/arial.ttf", 100);

		
	return panels ;
}

void setupPlugins(std::vector<std::shared_ptr<AsyncPlugin>>& plugins, const std::string& app_title, const std::string& command) {


	std::shared_ptr<SavePlugin> files(new SavePlugin());
	addTool(files);
	std::shared_ptr<SteamworksPlugin> steamworks(new SteamworksPlugin(3485250, command));
	addTool(steamworks);
	std::shared_ptr<AudioPlugin> sound_system(new AudioPlugin());
	addTool(sound_system);
	std::shared_ptr<VulkanPlugin> window(new VulkanPlugin(app_title, false, true)); // vsync, fullscreen
	addTool(window);
	std::shared_ptr<StatePlugin> app(new StatePlugin());
	addTool(app);
	std::shared_ptr<WorldPlugin> worlds(new WorldPlugin());
	addTool(worlds);
	std::shared_ptr<OpenXRPlugin> openXR(new OpenXRPlugin("./assets/controller_actions.json"));
	addTool(openXR);
	std::shared_ptr<ViewPlugin> view(new ViewPlugin());
	addTool(view);

	
	std::unordered_set<std::shared_ptr<RenderTarget>> render_targets ;

	//Set up the desktop window for the shaders we're gonna use
	int width = window->window_width;
	int height = window->window_height;
	auto window_target = createRenderTarget(width, height, window.get());
	window->setWindowTarget(window_target);
	render_targets.insert(window_target);
	printf("Window render target dimensions: %d x %d\n", width, height);
	//Set up the openXR render targets
	if (OpenXRPlugin::ENABLED) {
		auto res = openXR->getStereoTargetResolution();
		width = res.first;
		height = res.second;
		printf("VR render target dimensions: 2 x %d x %d\n", width, height);
		auto left_eye_target = createRenderTarget(width, height, window.get());
		auto right_eye_target = createRenderTarget(width, height, window.get());
		openXR->setStereoTargets(left_eye_target, right_eye_target);
		window->addRenderTarget(left_eye_target);
		window->addRenderTarget(right_eye_target);
		render_targets.insert(left_eye_target);
		render_targets.insert(right_eye_target);
	}

	Variant vertex_shader_file_data = Variant::loadFileBytes("./shader/VulkanParticle.vert.spv");
	Variant frag_shader_file_data = Variant::loadFileBytes("./shader/VulkanParticle.frag.spv");
	std::shared_ptr<ParticlePlugin> particles(new ParticlePlugin(window.get(), 2000, vertex_shader_file_data, frag_shader_file_data, render_targets));
	addTool(particles);
	std::shared_ptr<ScenePlugin> scene(setUpScene(window.get(), openXR.get()));
	addTool(scene);
	std::shared_ptr<PanelPlugin> panels(setUpPanels(window.get()));
	addTool(panels);

	plugins.push_back(openXR);
	plugins.push_back(window);
	plugins.push_back(worlds);
	plugins.push_back(app);
	plugins.push_back(sound_system);
	plugins.push_back(files);
	plugins.push_back(particles);
	plugins.push_back(scene);
	plugins.push_back(panels);
	plugins.push_back(steamworks);
	plugins.push_back(view);

}


void setupGameStates() {

	VulkanPlugin* window = getTool<VulkanPlugin>();
	OpenXRPlugin* xr = getTool<OpenXRPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>();
	StatePlugin* app = getTool<StatePlugin>();


	//app->add(VulkanDemoApp::state_name, std::shared_ptr<VulkanDemoApp>(new VulkanDemoApp()));
	//app->setState(VulkanDemoApp::state_name);

	//app->add(SceneDemoApp::state_name, std::shared_ptr<SceneDemoApp>(new SceneDemoApp()));
	//app->setState(SceneDemoApp::state_name);

	//app->add(SceneDemoApp2::state_name, std::shared_ptr<SceneDemoApp2>(new SceneDemoApp2()));
	//app->setState(SceneDemoApp2::state_name);

	//app->add(BallTestApp::state_name, std::shared_ptr<BallTestApp>(new BallTestApp()));
	//app->setState(BallTestApp::state_name);

	

	//app->add(SocketTest::state_name, std::shared_ptr<SocketTest>(new SocketTest()));
	//app->setState(SocketTest::state_name);

	//app->add(BallThrowApp::state_name, std::shared_ptr<BallThrowApp>(new BallThrowApp()));
	//app->setState(BallThrowApp::state_name);

	//app->add(MirrorApp::state_name, std::shared_ptr<MirrorApp>(new MirrorApp()));
	//app->setState(MirrorApp::state_name);

	//app->add(TraceApp::state_name, std::shared_ptr<TraceApp>(new TraceApp()));
	//app->setState(TraceApp::state_name);

	//app->add(CollisionTestApp::state_name, std::make_shared<CollisionTestApp>());
	//app->setState(CollisionTestApp::state_name);

	// app->add(ConstraintTestApp::state_name, std::shared_ptr<ConstraintTestApp>(new ConstraintTestApp()));
	// app->setState(ConstraintTestApp::state_name);

	//app->add(PyramidApp::state_name, std::shared_ptr<PyramidApp>(new PyramidApp()));
	//app->setState(PyramidApp::state_name);

	app->add(Chess::ChessApp::state_name, std::shared_ptr<Chess::ChessApp>(new Chess::ChessApp()));
	app->setState(Chess::ChessApp::state_name);
}

int debugMain(int argc, char* argv[]) {
	CSVLog::findDesync({"server.csv", "client.csv"}, "time", 1.0) ;
	
}

int exampleMain(int argc, char* argv[]) {
	std::string command_line = argv[0];
	for (int k = 1; k < argc; k++) {
		command_line += " " + std::string(argv[k]);
	}
	printf("Command: %s\n", command_line.c_str());

	SteamworksPlugin::enabled = true; // can turn this on when you've got your own steam app id you want to boot
	OpenXRPlugin::ENABLED = false ; // Enable this for VR support
	if (SteamworksPlugin::wants_to_exit) {
		printf("exiting because Steamworks plugin wanted to.\n");
		return 0;
	}

	std::shared_ptr<FlagSet> flag_set = std::shared_ptr<FlagSet>(new FlagSet(argc, argv));
	flag_set->setInt(AsyncPlugin::SHUTDOWN_FLAG, 0);
	addTool(flag_set);

	std::shared_ptr<ThreadSignals> thread_signals = std::shared_ptr<ThreadSignals>(new ThreadSignals());
	addTool(thread_signals);

	std::shared_ptr<ActionMap> action_map = std::shared_ptr<ActionMap>(new ActionMap());
	addTool(action_map);

	printf("Starting plugin construction...\n");
	std::vector<std::shared_ptr<AsyncPlugin>> plugins;
	setupPlugins(plugins, "Demo App", command_line);

	//initialize the plugins
	printf("Running plugin initializations...\n");
	for (auto& p : plugins) {
		p->initialize();
	}
	AsyncPlugin::runPlugins(plugins); // warm up the plugins so everything is in normal state for entering game state

	printf("Loading app states...\n");
	setupGameStates();

	printf("Starting main loop...\n");
	bool display_profile = false;



	auto last_frame_time = now();
	auto last_second_time = now();
	int frames = 0;

	long t = timeMilliseconds();

	if (display_profile) {
		VulkanPlugin* window = getTool<VulkanPlugin>();
		window->enableRenderTiming(concat("./performance_log_", t) + ".csv");
	}

	//WorldPlugin::enableEventLogging(concat("./event_log_", t) +" .csv", WorldPlugin::FINAL_EVENTS);
	//WorldPlugin::enableEventLogging(concat("./event_log_", t) + " .csv", concat("./extended_log_", t) + " .csv", WorldPlugin::FINAL_EVENTS);

	std::map<int, std::string > plugin_name;
	plugin_name[0] = "openXR";
	plugin_name[1] = "window";
	plugin_name[2] = "worlds";
	plugin_name[3] = "app";
	plugin_name[4] = "sound";
	plugin_name[5] = "files";
	plugin_name[6] = "particles";
	plugin_name[7] = "scene";
	plugin_name[8] = "panels";
	plugin_name[9] = "steam";
	
	AsyncPlugin::startPlugins(plugins);

	//Run main loop until told to stop
	while (flag_set->getInt(AsyncPlugin::SHUTDOWN_FLAG) == 0) {
		auto sync_start = now();
		bool ran = AsyncPlugin::runPlugins(plugins);
		long sync_time = microsBetween(sync_start, now());
		//Stagger the start of the plugins over the first third of the frame time
		//This makes the ideal execution order amd lowest input lag most likely, but they can still overlap if they need to to maintain fps
		int stagger_step = (int)(sync_time / (3 * plugins.size()));
		if(stagger_step > 1000){ // prevent death spiral from a single slow frame
			stagger_step = 0 ;
		}
		int stagger = 0;
		for (auto& p : plugins) {
			p->stagger_micros = stagger;
			stagger += stagger_step;
		}

		if (display_profile && ran) {
			frames++;
			std::shared_ptr<CSVLog>& log = VulkanPlugin::timing_log;
			int micros = microsBetween(last_second_time, now());
			if (micros > 1000000 * VulkanPlugin::log_print_interval_seconds) {
				//printf("Frames: %d\n", frames);
				for (int k = 0; k < plugins.size(); k++) {
					log->log(plugin_name[k] + " run", plugins[k]->run_time / frames);

					//printf("  %s - prep: %d (%.1f%%) run:%d  wait: %d (%.1f%%)\n", plugin_name[k].c_str(), plugins[k]->prep_time / frames, plugins[k]->prep_time * 100.0f / main_time, plugins[k]->run_time / frames, plugins[k]->wait_time / frames, (plugins[k]->async_enabled ? plugins[k]->wait_time : plugins[k]->run_time) * 100.0f / main_time);

					plugins[k]->run_time = 0;
				}
				//printf("  Total plugin time: %d (%.1f%%)\n", main_time, main_time * 100.f / micros);
				last_second_time = now();
				frames = 0;
			}
		}
	}

	printf("unlocking waiting threads...\n");
	thread_signals->signalAll();
	printf("joining threads...\n");
	AsyncPlugin::stopPlugins(plugins);

	// World Plugin makes more threads with its sockets that need to be cleaned up to not get an error on exit
	WorldPlugin* worlds = getTool<WorldPlugin>();
	SteamworksPlugin* steam = getTool<SteamworksPlugin>();
	if (worlds) {
		printf("Cleaning up sockets...\n");
		worlds->disconnect();
		steam->disconnect();
	}

	return 0;
}

int main(int argc, char* argv[]) {
	//Narball::main(argc, argv);
	exampleMain(argc, argv);
}