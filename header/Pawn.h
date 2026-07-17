#pragma once
#include "Piece.h"

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

        bool isValidMove(const glm::vec3& destination) const override {
            if (!Piece::isValidMove(destination)) return false;

            bool is_white = !!color;
            bool moved_towards_black = position.z - destination.z > 0.0f;
            float speed = fabs(position.z - destination.z);
            bool moved_valid_speed = speed == 1.0f || speed == 2.0f && !has_moved;

            return position.x == destination.x && is_white == moved_towards_black && moved_valid_speed && !blocked_by(destination);
        }
    };

    auto static getStructure(Pawn& obj) {
        return std::tie(obj.position, obj.model_name, obj.color, obj.board_id, obj.has_moved);
    };

}
