#pragma once
#include "Piece.h"
#include "Board.h"

namespace Chess {

    class Pawn :
        public Piece
    {
    public:

        Pawn(const glm::vec3& position, const int64_t& board_id, const Piece::COLOR& color)
            : Piece(position, board_id, color, std::string("pawn") + (color ? "_white" : "_black")) {};

        Pawn() = default;


        //This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
        // Just change the template parameter to match your class
        int getTypeId(Registry* r) const {
            return r->getIdForType<Pawn>();
        }

        bool isValidMove(const glm::vec3& destination) const override;

        // Checks self only, not enemy pawn
        bool tryingToEnPassant(const glm::vec3& destination) const override;

        bool tryingToPromote(const glm::vec3& destination) const override;
    };

    auto static getStructure(Pawn& obj) {
        return std::tie(obj.position, obj.model_name, obj.color, obj.board_id, obj.has_moved, obj.moved_count, obj.last_moved_position, obj.last_moved_turn);
    };

}
