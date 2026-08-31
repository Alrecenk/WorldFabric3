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
		game_over = false;
		select_promotion = false;
		most_recent_promotion_square = glm::vec3(0);

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

	// ghetto ass win animation until we bedazzle it more
	void Board::gameOver(const Piece::COLOR& color) {
		WorldPlugin* world = getTool<WorldPlugin>();
		
		for (const auto& i : {4.5, 3.5, 2.5, 1.5, 0.5}) {
			addPiece<King>(glm::vec3(4.5, 0, i), color);
			addPiece<King>(glm::vec3(-4.5, 0, i), color);
			addPiece<King>(glm::vec3(4.5, 0, -1.0 * i), color);
			addPiece<King>(glm::vec3(-4.5, 0, -1.0 * i), color);
		}
	}

	void Board::setPiecePosition(const glm::vec3& old_p, const glm::vec3& new_p) {
		WorldPlugin* world = getTool<WorldPlugin>();
		int64_t piece_id = board_of_pieces.at(old_p);
		auto maybe_piece = board_of_pieces.find(new_p); // being captured

		// Take/Destroy the piece being captured
		if (maybe_piece != board_of_pieces.end()) {
			std::println("Piece<{}> at {} is taking Piece<{}> at {}", piece_id, old_p, maybe_piece->second, new_p);
			queue(maybe_piece->second, time, &Piece::destroy);

			auto king = world->observe<King>("chess", maybe_piece->second);
			if (king) { // The king has been captured
				auto winner = world->observe<Piece>("chess", piece_id); // Assume capturing piece exists

				std::println("THE KING HAS FALLEN. GAME OVER.");
				game_over = true;
				queue(id, time, &Board::gameOver, winner->color);
			}
		}

		// Move the capturer into it's place
		board_of_pieces.erase(old_p);
		board_of_pieces.erase(new_p);
		board_of_pieces.emplace(new_p, piece_id);
		queue(piece_id, time, &Piece::setPosition, new_p);
		std::println("Moving Piece<{}> from {} to {}", piece_id, old_p, new_p);
	}

	void Board::promote(const glm::vec3& old_p, const glm::vec3& new_p) {
		WorldPlugin* world = getTool<WorldPlugin>();
		auto piece = world->observe<Piece>("chess", board_of_pieces.at(new_p));
		const int z_offset = piece->color ? -1 : 1;

		// setup selectable options for promotion
		glm::vec3 queen_p = new_p;
		queen_p.z += z_offset;
		queen_p.x += 2;
		glm::vec3 bishop_p = new_p;
		bishop_p.z += z_offset;
		bishop_p.x += 1;
		glm::vec3 knight_p = new_p;
		knight_p.z += z_offset;
		glm::vec3 rook_p = new_p;
		rook_p.z += z_offset;
		rook_p.x += -1;

		addPiece<Queen>(queen_p, piece->color);
		addPiece<Bishop>(bishop_p, piece->color);
		addPiece<Knight>(knight_p, piece->color);
		addPiece<Rook>(rook_p, piece->color);

		// Don't pass the turn, require this player to first make thier promotion choice
		select_promotion = true; 
		most_recent_promotion_square = new_p;
	}

	void Board::clearPromotionSelection() {
		WorldPlugin* world = getTool<WorldPlugin>();
		auto board = world->observeNearest<Board>("chess");
		const int z_offset = board->most_recent_promotion_square.z < 0 ? -1 : 1;

		for (const auto& x_offset : {-1, 0, 1, 2}) {
			glm::vec3 selection_piece_pos = board->most_recent_promotion_square;
			selection_piece_pos.z += z_offset;
			selection_piece_pos.x += x_offset;

			takePiece(selection_piece_pos);
		}

		select_promotion = false;
	}

	void Board::takePiece(const glm::vec3& piece) {
		auto maybe_piece = board_of_pieces.find(piece);
		if (maybe_piece != board_of_pieces.end()) {
			std::println("Piece<{}> at {} is being taken", maybe_piece->second, piece);
			queue(maybe_piece->second, time, &Piece::destroy);
			board_of_pieces.erase(piece);
		}
	}

	void Board::createBlackGlove() {
		if (glove_black_id == -1) {
			glove_black_id = create(std::shared_ptr<Glove>(new Glove(glm::vec3(.5, 0, 3.5), "glove", false)), time);
		}
	}

	/*
	* ==============================================================================================================
	* ^^^^^^^^^^^^^^^^^^^^^^^^^ Board ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	* ==============================================================================================================
	* ==============================================================================================================
	* ==============================================================================================================
	* ==============================================================================================================
	* ==============================================================================================================
	* ==============================================================================================================
	* ==============================================================================================================
	* vvvvvvvvvvvvvvvvvvvvvvvvv Board View vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	* ==============================================================================================================
	*/

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
		auto board = world->observeNearest<Board>("chess");

		if (tryingToSelectPromotion(piece->position)) {
			selectPromotion(piece);
		} else if (piece->tryingToCastle(destination)) {
			castle(destination, piece);
		} else if (piece->tryingToEnPassant(destination)) {
			enPassant(destination, piece);
		} else if (piece->tryingToPromote(destination)) {
			startPromotion(destination, piece);
		} else if (piece->isValidMove(destination)) {
			world->queue("chess", last_observation->id, &Board::setPiecePosition, piece->position, destination);
			world->queue("chess", last_observation->id, &Board::nextTurn);
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

	bool BoardView::tryingToSelectPromotion(const glm::vec3& piece) {
		WorldPlugin* world = getTool<WorldPlugin>();
		auto board = world->observeNearest<Board>("chess");
		return board->select_promotion && fabs(piece.z) == 4.5f;
	}

	void BoardView::selectPromotion(std::shared_ptr<const Chess::Piece>& piece) {
		WorldPlugin* world = getTool<WorldPlugin>();
		auto board = world->observe<Board>("chess", last_observation->id);

		world->queue("chess", last_observation->id, &Board::setPiecePosition, piece->position, board->most_recent_promotion_square);
		world->queue("chess", last_observation->id, &Board::clearPromotionSelection);
		world->queue("chess", last_observation->id, &Board::nextTurn);
	}

	void BoardView::startPromotion(glm::vec3& destination, std::shared_ptr<const Chess::Piece>& pawn) {
		WorldPlugin* world = getTool<WorldPlugin>();
		bool is_white = !!pawn->color;
		auto board = world->observeNearest<Board>("chess");

		world->queue("chess", last_observation->id, &Board::setPiecePosition, pawn->position, destination);
		world->queue("chess", last_observation->id, &Board::promote, pawn->position, destination);
		// Waiting to pass the turn once the player has picked a promotion piece in BoardView::selectPromotion...
	}

	// Assume the board state is perfectly setup currently for an en passant
	void BoardView::enPassant(glm::vec3& destination, std::shared_ptr<const Chess::Piece>& pawn) {
		WorldPlugin* world = getTool<WorldPlugin>();
		bool is_white = !!pawn->color;
		glm::vec3 enemy_pawn_pos = destination;
		enemy_pawn_pos.z += is_white ? 1 : -1;
		auto board = world->observeNearest<Board>("chess");

		world->queue("chess", last_observation->id, &Board::setPiecePosition, pawn->position, destination);
		world->queue("chess", last_observation->id, &Board::takePiece, enemy_pawn_pos);
		world->queue("chess", last_observation->id, &Board::nextTurn);
	}

	// The destination is the square the king is trying to castle to. Assume it's valid.
	void BoardView::castle(glm::vec3& destination, std::shared_ptr<const Chess::Piece>& king) {
		WorldPlugin* world = getTool<WorldPlugin>();
		bool trying_to_castle_east = destination.x > 0;
		float rook_x = trying_to_castle_east ? 3.5f : -3.5f;
		glm::vec3 rook_pos = glm::vec3(rook_x, 0, king->position.z);
		int64_t rook_id = king->piece_at(rook_pos);
		auto rook = world->observe<Rook>("chess", rook_id);

		if (rook && !king->has_moved && !rook->has_moved && !king->blocked_by(rook_pos)) {
			world->queue("chess", rook_id, &Rook::castle);
			world->queue("chess", last_observation->id, &Board::setPiecePosition, king->position, destination);
			world->queue("chess", last_observation->id, &Board::nextTurn);
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