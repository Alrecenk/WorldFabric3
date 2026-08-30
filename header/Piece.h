#ifndef _Piece_Piece_H_
#define _Piece_Piece_H_ 1


#include "WorldPlugin.h"
#include "ScenePlugin.h"
#include "PanelPlugin.h"
#include "ChessMouseAction.h"
#include <stdio.h>
#include <cstdlib>

#include <string>
#include <map>
#include <set>
#include <print>

namespace Chess {

class Piece : public WorldObject {
public:

	enum COLOR { black, white }; // never change the order of these lol!
	std::string model_name;
	COLOR color;
	int64_t board_id;
	bool has_moved;
	int moved_count;
	glm::vec3 last_moved_position;
	int last_moved_turn;


	Piece(const glm::vec3& position, const int64_t& board_id, const COLOR& color, const std::string& model_name)
		: WorldObject(position), board_id(board_id), color(color), model_name(model_name), has_moved(false), moved_count(0), last_moved_turn(-1) {};

	//Functions to be used as events must be void return and only const& parameters
	// Also they're not allowed to read or write any data outside the object except through timeline functions
	void setPosition(const glm::vec3& p);
	void destroy();
	std::vector<glm::vec3> squaresBetween(const glm::vec3& to_p) const;
	int64_t blocked_by(const glm::vec3& to_p) const;
	int64_t piece_at(const glm::vec3& to_p) const;
	bool moved_like_rook(const glm::vec3& destination) const;
	bool moved_like_bishop(const glm::vec3& destination) const;

	//Functions used on observables or on read objects need to be const
	void print() const override {};

	Piece() = default; // WorldObject's need a default constructor to make an object to deserialize into
	virtual ~Piece() = default; // Force to be polymorphic just in case

	virtual bool isValidMove(const glm::vec3& destination) const = 0;
	virtual bool tryingToCastle(const glm::vec3& destination) const { return false; }
	virtual bool tryingToEnPassant(const glm::vec3& destination) const { return false; }
	virtual bool tryingToPromote(const glm::vec3& destination) const { return false; }
};


class PieceView : public ObjectView<Piece>, public virtual ActionReceiver<ChessMouseAction> {
public:
	int scene_id = -1;
	int trigger_id = -1;
	glm::mat4 pose;
	std::shared_ptr<const Piece> last_observation;

	//created is called when an objectis observed that ws no observed last time view was called on the world
	void created(std::shared_ptr<const Piece>& observation) override;

	//Update is called when an observation is made of an object that was also observed last frame on this same view
	void updated(std::shared_ptr<const Piece>& observation) override;

	//Destroyed is called when an observation that was present in the last observation is no longer observed
	//This view will be deleted immediately after this call (it's destructor will be called after this)
	void destroyed() override;

	void receiveAction(std::shared_ptr<ChessMouseAction>& action, std::shared_ptr<ActionTrigger>& trigger) override;
};


} // end Piece name space

template <>
struct std::formatter<Chess::Piece::COLOR> {
	auto format(const Chess::Piece::COLOR& c, std::format_context& ctx) const {
		return std::format_to(ctx.out(), "{}", c ? "White" : "Black");
	}
	constexpr auto parse(std::format_parse_context& ctx) {
		return ctx.begin();
	}
};

template <>
struct std::formatter<Chess::Piece> {
	auto format(const Chess::Piece& p, std::format_context& ctx) const {
		// This is the only line that matters, the rest is boiler plate, to get std::println working
		return std::format_to(ctx.out(), "(Piece <{}> {} {} at ({},{}), moved:{} on board <{}>)", p.id, p.color, p.model_name, p.position.x, p.position.z, p.has_moved, p.board_id);
	}
	constexpr auto parse(std::format_parse_context& ctx) {
		return ctx.begin();
	}
};

#endif // #ifndef _Piece_Piece_H_