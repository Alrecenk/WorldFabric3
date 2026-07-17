#ifndef _NARBALL_MAIN_H_
#define _NARBALL_MAIN_H_ 1
#pragma once

#include "AsyncPlugin.h"
#include "FlagSet.h"
#include "OpenXRPlugin.h"
#include "ScenePlugin.h"
#include "AudioPlugin.h"
#include "ParticlePlugin.h"
#include "GLTF.h"
#include "SavePlugin.h"
#include "StatePlugin.h"
#include "WorldPlugin.h"
#include "SteamworksPlugin.h"
#include "NarballGame.h"
#include "Timeline.h"
#include "VulkanPlugin.h"
#include "ThreadSignals.h"
#include "PanelPlugin.h"
#include "CSVLog.h"
#include "Utilities.h"
#include "NarballMenu.h"
#include "NarballServer.h"
#include "ViewPlugin.h"

#include <vector>
#include <set>
#include <memory>
#include <string>
#include <chrono>
#include <thread>

using std::string;

namespace Narball{

enum AppType { GAME, DEDICATED_SERVER, DESYNC_CHECKER} ;

inline AppType which_app = GAME;

inline std::shared_ptr<RenderTarget> createRenderTarget(int width, int height, VulkanPlugin* window) {

	VkClearColorValue background_color = { 0.0f,1.0f,0.0f,1.0f };
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
	final_image->requires_clearing = false;
	color_image->requires_clearing = false; // we can skip clearing these because clearing point_image is enough to know if we're lookin at old data in post processing
	normal_image->requires_clearing = false;
	std::shared_ptr<WFImage> panel_image = std::shared_ptr<WFImage>(new WFImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, drawImageUsages));

	std::shared_ptr<WFImage> depth_image = std::shared_ptr<WFImage>(new WFImage(width, height, VK_FORMAT_D32_SFLOAT, 
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));

	std::shared_ptr<RenderTarget> target = std::shared_ptr<RenderTarget>(new RenderTarget());

	target->setImages({ color_image, normal_image, point_image,final_image, panel_image }, { background_color, background_normal, background_point, start_light, panel_background }, depth_image, color_image); // panel plugin writes from final image back int ocolor image for its blur effect so final image is in color image
	//target->createExtendedFragments(4, 8, window); // Narball doesn't  use extended fragments for translucency
	target->enableScreenResize(true);
	return target;
}



inline std::pair< std::shared_ptr<TriangleShaderProgram>, std::shared_ptr<TriangleShaderProgram>> loadSceneShader(ScenePlugin* scene, VulkanPlugin* window, const std::string& vert_main, const std::string& frag_main, const std::string& vert_shadow, const std::string& frag_shadow) {

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


	return { mesh_program, shadow_program };

}

inline std::pair< std::shared_ptr<TriangleShaderProgram>, std::shared_ptr<TriangleShaderProgram>> loadTranslucentSceneShader(ScenePlugin* scene, VulkanPlugin* window, const std::string& vert_main, const std::string& frag_main, const std::string& vert_shadow, const std::string& frag_shadow) {

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
		sizeof(ScenePlugin::TranslucentPushConstants),
		num_textures,
		VK_CULL_MODE_FRONT_BIT,
		scene->getAShadowTarget(),
		OVERWRITE
	));
	vkDestroyShaderModule(window->device, shadow_vertex_shader, nullptr);
	vkDestroyShaderModule(window->device, shadow_frag_shader, nullptr);

	return { mesh_program, shadow_program };

}


inline ScenePlugin* setUpScene(VulkanPlugin* window, OpenXRPlugin* xr) {
	ScenePlugin* scene = new ScenePlugin(window, xr);

	Variant light_shader_file_data;
	light_shader_file_data = Variant::loadFileBytes("./Narball/shader/NarballLightMapPost.comp.spv");
	VkShaderModule light_shader = window->loadShader(light_shader_file_data.getByteArray(), light_shader_file_data.getArrayLength());
	scene->setLightProgram(light_shader);
	vkDestroyShaderModule(window->device, light_shader, nullptr);

	NarballLightComponent lc;
	lc.light_color = glm::vec4(0.7, 0.7, 0.7, 1);
	scene->createLight<ScenePlugin::ScreenPushConstants, NarballLightComponent>(glm::vec3(-5, 15, -5), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0), 0.55f, 30, 1024, 0, lc);

	auto shaders1 = loadSceneShader(scene, window, "./Narball/shader/GLTF1.vert.spv", "./Narball/shader/GLTF.frag.spv", "./Narball/shader/GLTFShadow1.vert.spv", "./Narball/shader/GLTFShadow.frag.spv");
	scene->addDefaultShader<ScenePlugin::DefaultPushConstants, GLTF::Instance1>(shaders1.first, shaders1.second, 1);

	auto shaders64 = loadSceneShader(scene, window, "./Narball/shader/GLTF64.vert.spv", "./Narball/shader/GLTF.frag.spv", "./Narball/shader/GLTFShadow64.vert.spv", "./Narball/shader/GLTFShadow.frag.spv");
	scene->addDefaultShader<ScenePlugin::DefaultPushConstants, GLTF::Instance64>(shaders64.first, shaders64.second, 64);

	auto shaders256 = loadSceneShader(scene, window, "./Narball/shader/GLTF256.vert.spv", "./Narball/shader/GLTF.frag.spv", "./Narball/shader/GLTFShadow256.vert.spv", "./Narball/shader/GLTFShadow.frag.spv");
	scene->addDefaultShader<ScenePlugin::DefaultPushConstants, GLTF::Instance256>(shaders256.first, shaders256.second, 256);
/*
	auto shadersblend1 = loadTranslucentSceneShader(scene, window, "./Narball/shader/GLTF1Blend.vert.spv", "./Narball/shader/GLTFBlend.frag.spv", "./Narball/shader/GLTFShadowBlend1.vert.spv", "./Narball/shader/GLTFShadowBlend.frag.spv");
	scene->addDefaultShader<ScenePlugin::TranslucentPushConstants, GLTF::Instance1>(shadersblend1.first, shadersblend1.second, 1, true);
*/
	return scene;
}

inline PanelPlugin* setUpPanels(VulkanPlugin* window) {
	PanelPlugin* panels = new PanelPlugin(window);

	//Crate an example target that has the image layout of a panel to give to the element shader
	VkImageUsageFlags drawImageUsages = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	std::shared_ptr<WFImage> color_image = std::shared_ptr<WFImage>(new WFImage(16, 16, VK_FORMAT_R8G8B8A8_UNORM, drawImageUsages));
	std::shared_ptr<WFImage> depth_image = std::shared_ptr<WFImage>(new WFImage(16, 16, VK_FORMAT_D32_SFLOAT, 
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
	std::shared_ptr<RenderTarget> example_target = std::shared_ptr<RenderTarget>(new RenderTarget());
	example_target->setImages({ color_image }, { {0,0,0,1} }, depth_image, color_image);

	// Load the shader for drawing elements into panels
	Variant vertex_shader_file_data2 = Variant::loadFileBytes("./Narball/shader/PanelElement.vert.spv");
	VkShaderModule triangleVertexShader2 = window->loadShader(vertex_shader_file_data2.getByteArray(), vertex_shader_file_data2.getArrayLength());
	Variant frag_shader_file_data2 = Variant::loadFileBytes("./Narball/shader/PanelElement.frag.spv");
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
	Variant vertex_shader_file_data = Variant::loadFileBytes("./Narball/shader/Panel.vert.spv");
	VkShaderModule triangleVertexShader = window->loadShader(vertex_shader_file_data.getByteArray(), vertex_shader_file_data.getArrayLength());
	Variant frag_shader_file_data = Variant::loadFileBytes("./Narball/shader/Panel.frag.spv");
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
	Variant screen_file_data = Variant::loadFileBytes("./Narball/shader/PanelPost.comp.spv");
	VkShaderModule computeShader = window->loadShader(screen_file_data.getByteArray(), screen_file_data.getArrayLength());
	std::shared_ptr<ScreenShaderProgram> screen_program = std::shared_ptr<ScreenShaderProgram>(new ScreenShaderProgram(window->device, computeShader, sizeof(PanelPlugin::ScreenPushConstants), window->window_target->images, 16));

	vkDestroyShaderModule(window->device, computeShader, nullptr);

	PanelPlugin::DefaultInstance first_post_component;
	panels->setShaders<PanelPlugin::DefaultPushConstants, PanelPlugin::DefaultInstance, PanelPlugin::DefaultPushConstants, PanelPlugin::DefaultInstance, PanelPlugin::ScreenPushConstants, PanelPlugin::DefaultInstance>(
		element_program, panel_program, screen_program, first_post_component);

	panels->addFont("arial100", "./Narball/asset/arial.ttf", 100);
	panels->addFont("arial75", "./Narball/asset/arial.ttf", 75);
	panels->addFont("arial60", "./Narball/asset/arial.ttf", 60);
	panels->addFont("arial50", "./Narball/asset/arial.ttf", 50);
	panels->addFont("arial24", "./Narball/asset/arial.ttf", 24);
	panels->addFont("arial12", "./Narball/asset/arial.ttf", 12);

	return panels;
}

inline void setupPlugins(std::vector<std::shared_ptr<AsyncPlugin>>& plugins, const std::string& app_title, std::string command ) {

	//The file plugin needs to load first, so it can load config options from the game save to be used with other plugins initialization
	std::shared_ptr<SavePlugin> files(new SavePlugin()); 
	addTool(files);
	std::shared_ptr<SteamworksPlugin> steamworks(new SteamworksPlugin(4404880, command));
	addTool(steamworks);
	std::shared_ptr<AudioPlugin> sound_system(new AudioPlugin());
	addTool(sound_system);
	std::shared_ptr<VulkanPlugin> window(new VulkanPlugin(app_title, true, true)); // vsync, fullscreen
	addTool(window);
	std::shared_ptr<StatePlugin> app(new StatePlugin());
	addTool(app) ;
	std::shared_ptr<WorldPlugin> worlds( new WorldPlugin());
	addTool(worlds);
	std::shared_ptr<OpenXRPlugin> openXR(new OpenXRPlugin("./Narball/asset/controller_actions.json"));
	addTool(openXR);
	

	std::unordered_set<std::shared_ptr<RenderTarget>> render_targets;

	//Set up the desktop window with render targets for the shaders we're gonna use
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

	Variant vertex_shader_file_data = Variant::loadFileBytes("./Narball/shader/VulkanParticle.vert.spv");
	Variant frag_shader_file_data = Variant::loadFileBytes("./Narball/shader/VulkanParticle.frag.spv");
	std::shared_ptr<ParticlePlugin> particles(new ParticlePlugin(window.get(), 2000, vertex_shader_file_data, frag_shader_file_data, render_targets));
	addTool(particles);
	std::shared_ptr<ScenePlugin> scene(setUpScene( window.get(), openXR.get())) ;
	addTool(scene);
	std::shared_ptr<PanelPlugin> panels (setUpPanels(window.get()));
	addTool(panels);

	std::shared_ptr<ViewPlugin> view (new ViewPlugin());

	plugins.push_back(openXR);
	plugins.push_back(window);
	plugins.push_back(app);
	plugins.push_back(steamworks);
	plugins.push_back(worlds);
	plugins.push_back(files);
	plugins.push_back(view);
	plugins.push_back(sound_system);
	plugins.push_back(particles);
	plugins.push_back(scene);
	plugins.push_back(panels);
	
}

inline void setupGameStates() {

	VulkanPlugin* window = getTool<VulkanPlugin>();
	OpenXRPlugin* xr = getTool<OpenXRPlugin>();
	ScenePlugin* scene = getTool<ScenePlugin>();
	ParticlePlugin* particles = getTool<ParticlePlugin>();
	WorldPlugin* worlds = getTool<WorldPlugin>();
	StatePlugin* app = getTool<StatePlugin>();

	app->add(NarballMenu::state_name, std::shared_ptr<NarballMenu>(new NarballMenu()));
	app->add(NarballGame::state_name, std::shared_ptr<NarballGame>(new NarballGame()));
	
	//app->setState(NarballTest::state_name);
	app->setState(NarballMenu::state_name);
}

inline void registerWorldObjects(WorldPlugin* worlds){
	//Register all of our timeline objects and their functions, so they can be auto-synced with the WorldPlugin
	worlds->registerClass<Narwhal,NarwhalView>("Narwhal");
	worlds->registerMethod(&Narwhal::setPosition, "setPosition");
	worlds->registerMethod(&Narwhal::update, "update");
	worlds->registerMethod(&Narwhal::setVelocity, "setVelocity");
	worlds->registerMethod(&Narwhal::applyImpulse, "applyImpulse");
	worlds->registerMethod(&Narwhal::setControls, "setControls");
	worlds->registerMethod(&Narwhal::changeColor, "changeColor");

	worlds->registerClass<Ball, BallView>("Ball");
	worlds->registerMethod(&Ball::setPosition, "setPosition");
	worlds->registerMethod(&Ball::update, "update");
	worlds->registerMethod(&Ball::setVelocity, "setVelocity");
	worlds->registerMethod(&Ball::applyImpulse, "applyImpulse");
	worlds->registerMethod(&Ball::narwhalHit, "narwhalHit");
	worlds->registerMethod(&Ball::destroy, "destroy");

	worlds->registerClass<Cell>("Cell");
	worlds->registerMethod(&Cell::add, "add");
	worlds->registerMethod(&Cell::remove, "remove");
	worlds->registerMethod(&Cell::destroyBalls, "destroyBalls");
	worlds->registerMethod(&Cell::destroy, "destroy");

	worlds->registerClass<Match>("Match");
	worlds->registerMethod(&Match::createMatch, "createGird");
	worlds->registerMethod(&Match::scorePoints, "scorePoints");
	worlds->registerMethod(&Match::update, "update");
	worlds->registerMethod(&Match::closeMatch, "closeMatch");


	worlds->registerClass<Lobby>("Lobby");
	worlds->registerMethod(&Lobby::setMatchParameters, "setMatchParamters");
	worlds->registerMethod(&Lobby::addPlayer, "addPlayer");
	worlds->registerMethod(&Lobby::removePlayer, "removePlayer");
	worlds->registerMethod(&Lobby::setTeam, "setTeam");
	worlds->registerMethod(&Lobby::rewardPoint, "rewardPoint");
	worlds->registerMethod(&Lobby::switchTeam, "switchTeam");
	worlds->registerMethod(&Lobby::setState, "setState");
	worlds->registerMethod(&Lobby::setResult, "setResult");
	worlds->registerMethod(&Lobby::keepAlive, "keepAlive");
	worlds->registerMethod(&Lobby::kickDisconnected, "kickDisconnected");
	worlds->registerMethod(&Lobby::setTimeLobbyStarted, "setTimeLobbyStarted");
	worlds->registerMethod(&Lobby::setMinTimeInLobby, "setMinTimeInLobby");

}

inline void loadAssets(WorldPlugin* worlds, ScenePlugin* scene){

	
	registerWorldObjects(worlds);

	// Preload the models and sounds we're going to use for the whole game
	scene->createModelSet(narwhal_model[0], "./Narball/asset/Narwhal_tail.glb", false);
	scene->createModelSet(narwhal_model[1], "./Narball/asset/Narwhal_right_red.glb", false);
	scene->createModelSet(narwhal_model[2], "./Narball/asset/Narwhal_left_blue.glb", false);
	scene->createModelSet(ball_model, "./Narball/asset/BeachBall.glb", false);
	scene->createModelSet(pool_model, "./Narball/asset/Pool.glb", false);
	scene->createModelSet(beach_model, "./narball/asset/palmdiorama.glb", true);

	scene->addAnimation(tail_animation, "./Narball/asset/Narwhal_tail.glb", 0);
	scene->addAnimation(left_fin_animation, "./Narball/asset/Narwhal_left_blue.glb", 0);
	scene->addAnimation(right_fin_animation, "./Narball/asset/Narwhal_right_red.glb", 0);

	AudioPlugin* sound = getTool<AudioPlugin>();
	TAP_SOUND = sound->addWAV("./Narball/asset/tap.wav", SOUND_EFFECTS_GROUP);
	SCORE_SOUND = sound->addWAV("./Narball/asset/goodding1.wav", SOUND_EFFECTS_GROUP);
	WALL_HIT_SOUND = sound->addWAV("./Narball/asset/smack.wav", SOUND_EFFECTS_GROUP);
	sound_pitch_range[WALL_HIT_SOUND] = {0.4f,0.6f};
	BALL_HIT_SOUND = WALL_HIT_SOUND ; // TODO have unique sounds for different hits?
	sound_pitch_range[BALL_HIT_SOUND] = { 0.8f,1.3f };
	NARWHAL_HIT_SOUND = WALL_HIT_SOUND;
	sound_pitch_range[NARWHAL_HIT_SOUND] = { 0.5f,0.8f};
	BALL_BALL_SOUND = WALL_HIT_SOUND;
	sound_pitch_range[BALL_BALL_SOUND] = { 0.8f,1.3f };
	BALL_WALL_SOUND = WALL_HIT_SOUND;
	sound_pitch_range[BALL_WALL_SOUND] = { 0.7f,0.9f };
	sound->setGroupVolume(SOUND_EFFECTS_GROUP, 1);

	std::vector<AudioPlugin::Note> note = sound->pianoNote(48, 0, 0.4f, 0.25f) ; // sound plugin can generate basic electric piano notes
	AudioPlugin::SoundData count_sound = sound->compose(note) ;
	COUNTDOWN_SOUND = sound->addSound(count_sound, SOUND_EFFECTS_GROUP);


	std::vector<AudioPlugin::Note> notes = sound->pianoNote(50, 0, 0.3f, 0.4f); 
	note = sound->pianoNote(57, 0.1f, 0.5f, 0.5f);
	notes.insert(notes.end(), note.begin(), note.end());
	note = sound->pianoNote(62, 0.1f, 0.4f, 0.4f);
	notes.insert(notes.end(), note.begin(), note.end());
	AudioPlugin::SoundData end_sound = sound->compose(notes);// and compose chords
	ENDING_SOUND = sound->addSound(end_sound, SOUND_EFFECTS_GROUP);

	MENU_MUSIC = sound->addOGG("./Narball/asset/Apple Cider [Loop](mono).ogg", MUSIC_GROUP);
	MATCH_MUSIC = sound->addOGG("./Narball/asset/Course 1 [Loop](mono).ogg", MUSIC_GROUP);
	sound->setGroupVolume(MUSIC_GROUP, 0.5f);

}



inline int desyncMain(int argc, char* argv[]) {
	CSVLog::findDesync({ "event_log_1.csv", "event_log_2.csv" }, "time", 1.0);
	return 0 ;
}


inline int gameMain(int argc, char* argv[]) {
	std::string command_line =argv[0];
	for(int k = 1 ;k < argc;k++){
		command_line += " " + std::string(argv[k]) ;
	}
	printf("Command: %s\n", command_line.c_str()) ;

	
	if (SteamworksPlugin::wants_to_exit) {
		printf("exiting because Steamworks plugin wanted to.\n");
		return 0;
	}

	std::shared_ptr<FlagSet> flag_set = std::shared_ptr<FlagSet>(new FlagSet(argc, argv));
	flag_set->setInt(AsyncPlugin::SHUTDOWN_FLAG, 0);
	addTool(flag_set);

	std::shared_ptr<ThreadSignals> thread_signals = std::shared_ptr<ThreadSignals>(new ThreadSignals());
	addTool(thread_signals);

	printf("Starting plugin construction...\n");
	std::vector<std::shared_ptr<AsyncPlugin>> plugins;
	setupPlugins(plugins, "Narball Test", command_line);

	//initialize the plugins
	printf("Running plugin initializations...\n");
	for (auto& p : plugins) {
		p->initialize();
	}

	printf("Loading assets...\n");
	loadAssets(getTool<WorldPlugin>(), getTool<ScenePlugin>());

	printf("Loading app states...\n");
	setupGameStates();

	printf("Starting main loop...\n");
	bool display_profile = true;

	

	auto last_frame_time = now();
	auto last_second_time = now();
	int frames = 0;

	long t = timeMilliseconds();
 
	if (display_profile) {
		VulkanPlugin* window = getTool<VulkanPlugin>();
		std::string log_location = concat("./performance_log_", t) + ".csv";
		window->enableRenderTiming(log_location) ;
		printf("Perofmance profile Logging to %s.\n", log_location.c_str());
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
		auto sync_start = now() ;
		AsyncPlugin::runPlugins(plugins) ;
		long sync_time = microsBetween(sync_start, now());
		//Stagger the start of the plugins over the first third of the frame time
		//This makes the ideal execution order amd lowest input lag most likely, but they can still overlap if they need to to maintain fps
		int stagger_step = (int)(sync_time / (3*plugins.size()) );
		int stagger = 0;
		for (auto& p : plugins) {
			p->stagger_micros = stagger;
			stagger += stagger_step;
		}

		if (display_profile) {
			frames++;
			std::shared_ptr<CSVLog>& log = VulkanPlugin::timing_log ;
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
	thread_signals->signalAll() ;
	printf("joining threads...\n");
	AsyncPlugin::stopPlugins(plugins);

	// World Plugin makes more threads with its sockets that need to be cleaned up to not get an error on exit
	WorldPlugin* worlds = getTool<WorldPlugin>();
	if (worlds) {
		printf("Cleaning up sockets...\n");
		worlds->disconnect();
	}

	return 0;
}



inline int dedicatedMain(int argc, char* argv[]) {


	std::string command_line = argv[0];
	for (int k = 1; k < argc; k++) {
		command_line += " " + std::string(argv[k]);
	}
	printf("Command: %s\n", command_line.c_str());


	if (SteamworksPlugin::wants_to_exit) {
		printf("exiting because Steamworks plugin wanted to.\n");
		return 0;
	}

	std::shared_ptr<FlagSet> flag_set = std::shared_ptr<FlagSet>(new FlagSet(argc, argv));
	flag_set->setInt(AsyncPlugin::SHUTDOWN_FLAG, 0);
	addTool(flag_set);

	std::shared_ptr<ThreadSignals> thread_signals = std::shared_ptr<ThreadSignals>(new ThreadSignals());
	addTool(thread_signals);


	


	printf("Starting plugin construction...\n");
	std::vector<std::shared_ptr<AsyncPlugin>> plugins;
	
	Variant server_settings = Variant::loadJSONFile("./server_settings.json") ;
	
	SteamworksPlugin::SteamServerInfo info;
	info.name = server_settings["name"].getString();
	info.map = "The Pool";
	info.max_players = (int)round(server_settings["max_players"].getNumberAsFloat());
	info.game_mode = "Classic";
	info.product_name = "Narball";
	info.product_description = "Be a narwhal. Hit a ball.";
	info.game_directory = "Narball" ;
	info.port = (uint16)round(server_settings["port"].getNumberAsFloat());
	info.version = NARBALL_DEDICATED_VERSION ;
	NarballServer::num_balls = (int)round(server_settings["balls"].getNumberAsFloat());
	NarballServer::match_points = (int)round(server_settings["points"].getNumberAsFloat());
	NarballServer::min_players = (int)round(server_settings["min_players"].getNumberAsFloat());
	NarballServer::min_time_in_lobby= server_settings["min_time_in_lobby"].getNumberAsFloat();
	std::shared_ptr<SteamworksPlugin> steamworks(new SteamworksPlugin(4404880, info, 27016, command_line));
	addTool(steamworks);
	std::shared_ptr<StatePlugin> app(new StatePlugin());
	addTool(app);
	std::shared_ptr<WorldPlugin> worlds(new WorldPlugin());
	addTool(worlds) ;

	plugins.push_back(worlds);
	plugins.push_back(app);
	plugins.push_back(steamworks);

	//initialize the plugins
	printf("Running plugin initializations...\n");
	for (auto& p : plugins) {
		p->initialize();
	}
	printf("Registering world objects...\n");
	registerWorldObjects(worlds.get());

	printf("Loading app states...\n");
	app->add(NarballServer::state_name, std::shared_ptr<NarballServer>(new NarballServer()));
	app->setState(NarballServer::state_name);

	printf("Starting main loop...\n");
	//Run main loop until told to stop
	AsyncPlugin::startPlugins(plugins);

	//Run main loop until told to stop
	while (flag_set->getInt(AsyncPlugin::SHUTDOWN_FLAG) == 0) {
		AsyncPlugin::runPlugins(plugins);
	}

	printf("unlocking waiting threads...\n");
	thread_signals->signalAll();
	printf("joining threads...\n");
	AsyncPlugin::stopPlugins(plugins);

	// World Plugin makes more threads with its sockets that need to be cleaned up to not get an error on exit
	if (worlds) {
		worlds->disconnect();
	}

	debug_label.reset();// loose labels need to be cleaned up before plugin deconstruct to avoid error


	
	return 0;
}


inline int main(int argc, char* argv[]) {
	int result = -1 ;
	if(which_app == GAME){
		result = gameMain(argc, argv);
	}else if(which_app == DEDICATED_SERVER){
		result = dedicatedMain(argc, argv) ;
	}else if(which_app == DESYNC_CHECKER){
		result = desyncMain(argc, argv);
	}
	printf("Clean exit of main\n");
	return result ;
}


}// end name space narball

#endif // #ifndef _NARBALL_MAIN_H_