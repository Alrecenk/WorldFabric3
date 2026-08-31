#ifndef _Board_Board_H_
#define _Board_Board_H_ 1
#define GLM_ENABLE_EXPERIMENTAL


#include "WorldPlugin.h"
#include "ScenePlugin.h"
#include "PanelPlugin.h"
#include "ChessMouseAction.h"
#include <stdio.h>
#include <cstdlib>
#include <Piece.h>

#include <string>
#include <map>
#include <set>
#include <Glove.h>
#include <Rook.h>
#include <Pawn.h>
#include <King.h>
#include <Queen.h>
#include <Bishop.h>
#include <Knight.h>
#include <glm/gtx/hash.hpp> // Enables std::hash for glm vectors

namespace Chess {


	class Board : public WorldObject {
	public:

		std::string model_name;
		int64_t glove_white_id = -1;
		int64_t glove_black_id = -1;
		int turn_count;
		bool game_over;
		bool select_promotion;
		glm::vec3 most_recent_promotion_square;
		std::map<glm::vec3, int64_t, decltype([](glm::vec3 a, glm::vec3 b) {
			// Need to compare vec3's so the map can be ordered
			if (a.x != b.x) return a.x < b.x;
			if (a.y != b.y) return a.y < b.y;
			return a.z < b.z;
		})> board_of_pieces;

		Board(const glm::vec3& p, const std::string& model_name_set)
			: WorldObject(position), model_name(model_name_set) {};
		void init();

		void destroy();

		//Functions used on observables or on read objects need to be const
		void print() const override {}
		void printEvent();

		Board() {} // WorldObject's need a default constructor to make an object to deserialize into
		virtual ~Board() = default; // Force to be polymorphic just in case

		void createBlackGlove();

		template <typename T>
		void addPiece(const glm::vec3& p, const Piece::COLOR& color);

		void setPiecePosition(const glm::vec3& old_p, const glm::vec3& new_p);
		void promote(const glm::vec3& old_p, const glm::vec3& new_p);
		void takePiece(const glm::vec3& piece);
		void nextTurn();
		void clearPromotionSelection();
		void gameOver(const Piece::COLOR& color);

		//This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
		// Just change the template parameter to match your class
		int getTypeId(Registry* r) const {
			return r->getIdForType<Board>();
		}

	};

	auto static getStructure(Board& obj) {
		return std::tie(obj.position, obj.model_name, obj.glove_black_id, obj.glove_white_id, obj.board_of_pieces, obj.turn_count, obj.game_over);
	}


	class BoardView : public ObjectView<Board>, public virtual ActionReceiver<ChessMouseAction> {
	public:
		std::shared_ptr<const Board> last_observation;
		int scene_id = -1;
		int trigger_id = -1;
		int particle_id = -1 ;
		glm::mat4 pose;
		

		//created is called when an objectis observed that ws no observed last time view was called on the world
		void created(std::shared_ptr<const Board>& observation) override;

		//Update is called when an observation is made of an object that was also observed last frame on this same view
		void updated(std::shared_ptr<const Board>& observation) override;

		//Destroyed is called when an observation that was present in the last observation is no longer observed
		//This view will be deleted immediately after this call (it's destructor will be called after this)
		void destroyed() override;

		void receiveAction(std::shared_ptr<ChessMouseAction>& action, std::shared_ptr<ActionTrigger>& trigger) override;

		void glowUpHeldPiece(std::shared_ptr<const Piece>& piece) ;

		void castle(glm::vec3& destination, std::shared_ptr<const Chess::Piece>& piece);

		void handleGloves(glm::vec3& mouse_on_board_pos);

		void movePiece(std::shared_ptr<const Chess::Piece>& piece, glm::vec3& destination);

		void enPassant(glm::vec3& destination, std::shared_ptr<const Chess::Piece>& pawn);

		void startPromotion(glm::vec3& destination, std::shared_ptr<const Chess::Piece>& pawn);

		void selectPromotion(std::shared_ptr<const Chess::Piece>& piece);

		bool tryingToSelectPromotion(const glm::vec3& piece);
	};


} // end Board name space

template <>
struct std::formatter<Chess::Board> {
	auto format(const Chess::Board& p, std::format_context& ctx) const {
		// This is the only line that matters, the rest is boiler plate, to get std::println working
		return std::format_to(ctx.out(), "(Board <{}> {}, destroyed is {})", p.id, p.model_name, p.destroyed, p.turn_count);
	}
	constexpr auto parse(std::format_parse_context& ctx) {
		return ctx.begin();
	}
};

#endif // #ifndef _Board_Board_H_