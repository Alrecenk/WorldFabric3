#include "ChessApp.h"
#include "ParticlePlugin.h"
#include "ScenePlugin.h"
#include "FlagSet.h"
#include "ChessMouseAction.h"
#include <iostream>
#include <filesystem>


namespace Chess {
	ChessApp::ChessApp() {}

	void ChessApp::startSteamHosting() {
		SteamworksPlugin* steam = getTool<SteamworksPlugin>();
		WorldPlugin* world = getTool<WorldPlugin>();
		SteamworksPlugin::SteamServerInfo info;

		steam->setSteamEventReceiver(this);

		info.name = "Chess Server";
		info.map = "The Chess";
		info.max_players = 64;
		info.game_mode = "Classic";
		info.product_name = "Chess";
		info.product_description = "Be a piece. Hit a board.";
		info.game_directory = "Chess";
		info.version = "1";
		std::shared_ptr<Socket> steam_socket = steam->hostPrivateLobby(info);

		world->host(steam_socket, info.version);
	}


	// This is the client
	void ChessApp::onSteamGameExternalJoin(std::shared_ptr<SteamworksPlugin::SteamSocket> socket, const SteamworksPlugin::SteamServerInfo& server_info) {
		WorldPlugin* world = getTool<WorldPlugin>();
		SteamworksPlugin* steam = getTool<SteamworksPlugin>();

		mouse_action = std::shared_ptr<ChessMouseAction>(new ChessMouseAction()); // reset the held piece

		std::shared_ptr<Socket> steam_socket = socket; // casting by creation avoids a warning
		world->disconnect();
		world->connect(steam_socket, "1");

		std::println("{} <{}> joined via Steam", steam->getLocalName(), steam->getLocalSteamID());
	}


	std::vector<std::string> ChessApp::listFileStems(const std::string& path) {
		std::vector<std::string> files_list;

		try {
			for (const auto& entry : std::filesystem::directory_iterator(path)) {
				if (std::filesystem::is_regular_file(entry.status())) {
					files_list.push_back(entry.path().stem().string());
				}
			}
		}
		catch (const std::filesystem::filesystem_error& e) {
			std::cerr << "Error: " << e.what() << std::endl;
		}

		return files_list;
	}

	void ChessApp::registerClassesAndMethods() {
		WorldPlugin* world = getTool<WorldPlugin>();

		world->registerClass<Rook, PieceView>("Rook");
		world->registerMethod(&Rook::setPosition, "setPosition");
		world->registerMethod(&Rook::castle, "castle");
		world->registerClass<Pawn, PieceView>("Pawn");
		world->registerMethod(&Pawn::setPosition, "setPosition");
		world->registerClass<Queen, PieceView>("Queen");
		world->registerMethod(&Queen::setPosition, "setPosition");
		world->registerClass<King, PieceView>("King");
		world->registerMethod(&King::setPosition, "setPosition");
		world->registerClass<Bishop, PieceView>("Bishop");
		world->registerMethod(&Bishop::setPosition, "setPosition");
		world->registerClass<Knight, PieceView>("Knight");
		world->registerMethod(&Knight::setPosition, "setPosition");
		world->registerClass<Board, BoardView>("Board");
		world->registerMethod(&Board::init, "init");
		world->registerMethod(&Board::createBlackGlove, "createBlackGlove");
		world->registerMethod(&Board::setPiecePosition, "setPiecePosition");
		world->registerMethod(&Board::printEvent, "printEvent");
		world->registerClass<Glove, GloveView>("Glove");
		world->registerMethod(&Glove::setPosition, "setPosition");
		world->registerMethod(&Piece::destroy, "destroy");
	}

	void ChessApp::createWorldAndObjects() {
		WorldPlugin* world = getTool<WorldPlugin>();

		world->createWorld("chess", 10000, .0001f, 30);
		world->setTimeSpeed("chess", 1);

		//Make an instance of the board, which creates all the pieces
		std::shared_ptr<Board> board = std::shared_ptr<Board>(new Board(glm::vec3(0, 0, 0), "board"));
		int64_t board_id = world->create("chess", board);
		world->queue("chess", board_id, &Board::init);
	}

	void ChessApp::setupScene() {
		ScenePlugin* scene = getTool<ScenePlugin>();

		//Load all the game's blender models
		for (const auto& file_str : listFileStems("./assets/chess/glb/")) {
			scene->createModelSet(file_str, "./assets/chess/glb/" + file_str + ".glb");
		};

		// Set up a light for the scene
		ScenePlugin::LightComponent lc;
		glm::vec3 light_position = glm::vec3(4, 10, -10);
		lc.light_color = glm::vec4(0.9f, 0.9f, 0.9f, 1);
		light_effect_id = scene->createLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_position, light_look_at, glm::vec3(0, 1, 0), 0.7f, 20, 1024, 0, lc);
		scene->destroyLight(light_effect_id - 1); // delete default light
	}

	// Called when switching into this state before the first time run is called
	void ChessApp::enter(std::shared_ptr<MachineState> from) {
		//startSteamHosting();
		setupScene();
		registerClassesAndMethods();
		createWorldAndObjects();
	}

	//Called every frame while the state is active
	void ChessApp::run() {
		VulkanPlugin* window = getTool<VulkanPlugin>();
		WorldPlugin* world = getTool<WorldPlugin>();
		ActionMap* action_map = getTool<ActionMap>();

		mouse_action->origin = window->window_target->camera_position ;
		mouse_action->direction = window->getMouseRay() ;
		mouse_action->clicked = window->mouseDown(1) && !mouse_down_left ;
		action_map->performAction(mouse_action) ;
		mouse_action->held_piece = mouse_action->next_held_piece ;
		mouse_down_left = window->mouseDown(1) ;

		updateCamera();

		// Check if escape pressed to exit
		if (window->getLastKeyPress() == SDLK_ESCAPE) {
			getTool<FlagSet>()->setInt(AsyncPlugin::SHUTDOWN_FLAG, 1);
		}
	}

	// Called when switching out of this state after the last time run is called
	void ChessApp::exit(std::shared_ptr<MachineState> to) {
	}

	void ChessApp::updateCamera() {
		VulkanPlugin* window = getTool<VulkanPlugin>();
		ScenePlugin* scene = getTool<ScenePlugin>();
		if (window->mouseDown(3)) { // right mouse button
			if (!mouse_down_right) {
				mouse_down_position_right = window->getMousePosition();
				camera_down_thi = camera_thi;
				camera_down_theta = camera_theta;
			}
			glm::vec2 mouse_position = window->getMousePosition();
			mouse_down_right = true;
			camera_theta = camera_down_theta + camera_x_speed * (mouse_position.x - mouse_down_position_right.x);
			camera_thi = camera_down_thi + camera_y_speed * (mouse_position.y - mouse_down_position_right.y);
			camera_thi = fmax(fmin(camera_thi, 3.14159f * 0.5f), 0.0f);
			mouse_down_position_right = window->getMousePosition();
			camera_down_thi = camera_thi;
			camera_down_theta = camera_theta;

		} else {
			mouse_down_right = false;
		}

		if (mouse_wheel_y_previous < window->getMouseWheelPosition().y) { zoom *= 0.95f; }
		if (mouse_wheel_y_previous > window->getMouseWheelPosition().y) { zoom /= 0.95f; }

		if (zoom < 1.0f) { zoom = 1.0f;	}

		mouse_wheel_y_previous = window->getMouseWheelPosition().y;

		glm::vec3 camera_position = glm::vec3(cosf(camera_theta) * cosf(camera_thi), sinf(camera_thi), sinf(camera_theta) * cosf(camera_thi)) * zoom;
		window->window_target->setCamera(camera_position, look_at, fov, glm::vec3(0, 1, 0));

		glm::vec3 light_position = glm::vec3(cosf(camera_theta + light_theta_off) * cosf(camera_thi), sinf(camera_thi), sinf(camera_theta + light_theta_off) * cosf(camera_thi)) * light_zoom;

		scene->moveLight<ScenePlugin::ScreenPushConstants, ScenePlugin::LightComponent>(light_effect_id, light_position, light_look_at, glm::vec3(0, 1, 0), light_fov, 30);
	}

	float ChessApp::raytrace(const glm::vec3& pos, const glm::vec3& ray, int scene_id, glm::mat4 pose) {
		ScenePlugin* scene = getTool<ScenePlugin>();
		std::shared_ptr<GLTF> model = scene->getModelController(scene_id);
		if (!model) {
			return -1;
		}

		// Move the mouse ray into the model's coordinates
		glm::mat4 scene_to_model_space = glm::inverse(pose);
		glm::vec3 ray_origin = scene_to_model_space * glm::vec4(pos, 1); // positions have 1 in slot 4 to include translation
		glm::vec3 ray_direction = scene_to_model_space * glm::vec4(ray, 0); // directions have 0 in slot 4 to not include translation

		return model->rayTrace(ray_origin, ray_direction);
	}
}