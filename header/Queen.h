#pragma once
#include "Piece.h"

namespace Chess {

    class Queen :
        public Piece
    {
    public:

        Queen(const glm::vec3& position, const int64_t& board_id, const Piece::COLOR& color)
            : Piece(position, board_id, color, std::string("queen") + (color ? "_white" : "_black")) {};

        Queen() = default;


        //This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
        // Just change the template parameter to match your class
        int getTypeId(Registry* r) const {
            return r->getIdForType<Queen>();
        }

        bool isValidMove(const glm::vec3& destination) const override {
            if (!Piece::isValidMove(destination)) return false;

            return !blocked_by(destination) && (moved_like_rook(destination) || moved_like_bishop(destination));
        }
    };

    auto static getStructure(Queen& obj) {
        return std::tie(obj.position, obj.model_name, obj.color, obj.board_id, obj.has_moved);
    };

}
