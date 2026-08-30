#include "Board.h"

#include "VulkanPlugin.h"
#include "PanelPlugin.h"
#include "AudioPlugin.h"
#include "ChessApp.h"
#include "ParticlePlugin.h"
#include <print>

namespace Chess {
	void Board::destroy() {
		destroyed = true;
	}

	void Board::init() {
		WorldPlugin* world = getTool<WorldPlugin>();
		// Initialize all the chess pieces
		for (int i = 0; i < 8; i++) {
			addPiece<Pawn>(glm::vec3(i - 3.5, 0, -2.5), Piece::black);
			addPiece<Pawn>(glm::vec3(i - 3.5, 0, 2.5), Piece::white);
		}
		for (int i = 0; i < 2; i++) {
			addPiece<Knight>(glm::vec3(i * 5 - 2.5, 0, -3.5), Piece::black);
			addPiece<Knight>(glm::vec3(i * 5 - 2.5, 0, 3.5), Piece::white);
			addPiece<Rook>(glm::vec3(i * 7 - 3.5, 0, -3.5), Piece::black);
			addPiece<Rook>(glm::vec3(i * 7 - 3.5, 0, 3.5), Piece::white);
			addPiece<Bishop>(glm::vec3(i * 3 - 1.5, 0, -3.5), Piece::black);
			addPiece<Bishop>(glm::vec3(i * 3 - 1.5, 0, 3.5), Piece::white);
		}
		addPiece<King>(glm::vec3(.5, 0, -3.5), Piece::black);
		addPiece<King>(glm::vec3(.5, 0, 3.5), Piece::white);
		addPiece<Queen>(glm::vec3(-.5, 0, -3.5), Piece::black);
		addPiece<Queen>(glm::vec3(-.5, 0, 3.5), Piece::white);

		//This is this player's hand (the hosting aka first player)
		glove_white_id = create(std::shared_ptr<Glove>(new Glove(glm::vec3(.5, 0, 3.5), "glove")), time);

		turn_count = 0;

		world->queue("chess", id, &Board::printEvent);
	}

	void Board::nextTurn() {
		turn_count++;
	}

	void Board::printEvent() {
		WorldPlugin* world = getTool<WorldPlugin>();
		std::println("The board and all the pieces \n {}", *this);

		for (const auto& piece : board_of_pieces) {
			const std::shared_ptr<const Piece> p = world->observe<Piece>("chess", piece.second);
			std::println("{}", *p);
		}
	}

	template <typename T>
	void Board::addPiece(const glm::vec3& p, const Piece::COLOR& color) {
		board_of_pieces.emplace(p, create(std::shared_ptr<T>(new T(p, id, color)), time));
	}

	void Board::setPiecePosition(const glm::vec3& old_p, const glm::vec3& new_p) {
		// Take/Destroy the piece being captured
		auto maybe_piece = board_of_pieces.find(new_p);
		if (maybe_piece != board_of_pieces.end()) {
			std::println("Piece at {} is taking {} at {}", old_p, maybe_piece->second, new_p);
			queue(maybe_piece->second, time, &Piece::destroy);
		}

		// Move the capturer into it's place
		int64_t piece_id = board_of_pieces.at(old_p);
		board_of_pieces.erase(old_p);
		board_of_pieces.erase(new_p);
		board_of_pieces.emplace(new_p, piece_id);
		queue(piece_id, time, &Piece::setPosition, new_p);
		std::println("Moving {} from {} to {}", piece_id, old_p, new_p);
	}

	void Board::takePiece(const glm::vec3& p) {
		auto maybe_piece = board_of_pieces.find(p);
		if (maybe_piece != board_of_pieces.end()) {
			std::println("Piece<{}> at {} is being taken", maybe_piece->second, p);
			queue(maybe_piece->second, time, &Piece::destroy);
			board_of_pieces.erase(p);
		}
	}

	void Board::createBlackGlove() {
		if (glove_black_id == -1) {
			glove_black_id = create(std::shared_ptr<Glove>(new Glove(glm::vec3(.5, 0, 3.5), "glove", false)), time);
		}
	}

	void BoardView::created(std::shared_ptr<const Board>& observation) {
		ScenePlugin* scene = getTool<ScenePlugin>();
		ActionMap* action_map = getTool<ActionMap>();
		ParticlePlugin* particles = getTool<ParticlePlugin>();

		last_observation = observation;

		pose = glm::mat4(1.0f);
		pose = glm::translate(pose, observation->position);
		scene_id = scene->createInstance(observation->model_name, pose);

		std::shared_ptr<GLTF> model = scene->getModelController(observation->model_name);
		//Note: multiplying pose by AABB corners only works to prdouce another correct AABB here when pose contains only translation and scale
		std::shared_ptr<ActionTrigger> trigger = std::shared_ptr<ActionTrigger>(new ActionTrigger(0, pose * glm::vec4(model->min, 1), pose * glm::vec4(model->max, 1), this));
		trigger_id = action_map->addTrigger(trigger);

		particle_id = particles->createParticle(0);
	}

	//Update is called when an observation is made of an object that was also observed last frame on this same view
	void BoardView::updated(std::shared_ptr<const Board>& observation) {
		last_observation = observation;
	}

	//Destroyed is called when an observation that was present in the last observation is no longer observed
	//This view will be deleted immediately after this call (it's destructor will be called after this)
	void BoardView::destroyed() {
		ScenePlugin* scene = getTool<ScenePlugin>();
		ActionMap* action_map = getTool<ActionMap>();
		ParticlePlugin* particles = getTool<ParticlePlugin>();

		scene->deleteInstance(scene_id);
		action_map->removeTrigger(trigger_id);
		particles->destroyParticle(particle_id);
	}

	void BoardView::receiveAction(std::shared_ptr<ChessMouseAction>& action, std::shared_ptr<ActionTrigger>& trigger) {
		WorldPlugin* world = getTool<WorldPlugin>();
		float board_t = ChessApp::raytrace(action->origin, action->direction, scene_id, pose);
		glm::vec3 mouse_on_board_pos = action->origin + action->direction * board_t;
		mouse_on_board_pos.y = 0;
		glm::vec3 destination = mouse_on_board_pos; // Destination of a piece if placing a piece
		// Center piece inside its square
		destination.x = std::round(destination.x + .5f) - .5f;
		destination.z = std::round(destination.z + .5f) - .5f;

		handleGloves(mouse_on_board_pos);

		if (action->held_piece != -1) {
			std::shared_ptr<const Piece> piece = world->observe<Piece>("chess", action->held_piece);
			if (piece) {
				glowUpHeldPiece(piece);
			}
			if (action->clicked || !piece) {
				// Only send the network event if the position is different
				if (piece && (piece->position.x != destination.x || piece->position.z != destination.z)) {
					movePiece(piece, destination);
					world->queue("chess", last_observation->id, &Board::nextTurn);
				}
				action->next_held_piece = -1; // drop piece
			}
		}
		else {
			ParticlePlugin* particles = getTool<ParticlePlugin>();
			particles->setPose(particle_id, glm::mat4(0));
		}
	}

	void BoardView::movePiece(std::shared_ptr<const Chess::Piece>& piece, glm::vec3& destination) {
		WorldPlugin* world = getTool<WorldPlugin>();
		if (piece->tryingToCastle(destination)) {
			castle(destination, piece);
		} else if (piece->tryingToEnPassant(destination)) {
			enPassant(destination, piece);
		} else if (piece->isValidMove(destination)) {
			world->queue("chess", last_observation->id, &Board::setPiecePosition, piece->position, destination);
		}
	}

	void BoardView::handleGloves(glm::vec3& mouse_on_board_pos)	{
		WorldPlugin* world = getTool<WorldPlugin>();
		if (world->amHosting()) {
			world->queue("chess", last_observation->glove_white_id, &Glove::setPosition, mouse_on_board_pos);
		}
		else {
			if (last_observation->glove_black_id == -1) {
				world->queue("chess", last_observation->id, &Board::createBlackGlove);
			}
			else {
				world->queue("chess", last_observation->glove_black_id, &Glove::setPosition, mouse_on_board_pos);
			}
		}
	}

	// The destination is the square the pawn is trying to end up at after en passant. Assume it's valid.
	void BoardView::enPassant(glm::vec3& destination, std::shared_ptr<const Chess::Piece>& pawn) {
		WorldPlugin* world = getTool<WorldPlugin>();
		bool is_white = !!pawn->color;
		glm::vec3 enemy_pawn_pos = destination;
		enemy_pawn_pos.z += is_white ? 1 : -1;
		auto enemy_pawn = world->observe<Pawn>("chess", pawn->piece_at(enemy_pawn_pos));
		auto board = world->observeNearest<Board>("chess");

		if (enemy_pawn && enemy_pawn->moved_count == 1 && fabs(enemy_pawn->position.z) == 0.5f && enemy_pawn->last_moved_turn == board->turn_count - 1 && fabs(enemy_pawn->last_moved_position.z) == 2.5f) {
			world->queue("chess", last_observation->id, &Board::setPiecePosition, pawn->position, destination);
			world->queue("chess", last_observation->id, &Board::takePiece, enemy_pawn_pos);
		}
	}

	// The destination is the square the king is trying to castle to. Assume it's valid.
	void BoardView::castle(glm::vec3& destination, std::shared_ptr<const Chess::Piece>& king) {
		WorldPlugin* world = getTool<WorldPlugin>();
		bool trying_to_castle_east = destination.x > 0;
		float rook_x = trying_to_castle_east ? 3.5f : -3.5f;
		glm::vec3 rook_pos = glm::vec3(rook_x, 0, king->position.z);
		int64_t rook_id = king->piece_at(rook_pos);
		auto rook = world->observe<Rook>("chess", rook_id);

		if (rook) {
			bool can_castle = !king->has_moved && !rook->has_moved && !king->blocked_by(rook_pos);

			if (can_castle) {
				world->queue("chess", rook_id, &Rook::castle);
				world->queue("chess", last_observation->id, &Board::setPiecePosition, king->position, destination);
			}
		}
	}

	void BoardView::glowUpHeldPiece(std::shared_ptr<const Piece>& piece) {
		ParticlePlugin* particles = getTool<ParticlePlugin>();
		glm::mat4 particle_pose = glm::mat4(1.0f);
		particles->setColor(particle_id, glm::vec4(1, 1, .5, .5)); // glow
		particle_pose = glm::translate(particle_pose, piece->position);
		particle_pose = glm::scale(particle_pose, glm::vec3(.5f, .1f, .5f));
		particles->setPose(particle_id, particle_pose);
	}
}