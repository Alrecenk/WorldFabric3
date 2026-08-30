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
            bool moved_forward = is_white == moved_towards_black;
            float speed = fabs(position.z - destination.z);
            bool moved_valid_speed = speed == 1.0f || speed == 2.0f && !has_moved;
            bool moved_diagonal = fabs(destination.x - position.x) == 1.0f && fabs(position.z - destination.z) == 1.0f;
            bool is_capture = !!piece_at(destination);


            return moved_forward && position.x == destination.x && moved_valid_speed && !blocked_by(destination) && !is_capture || moved_forward && is_capture && moved_diagonal;
        }

        // Checks self only, not enemy pawn
        bool tryingToEnPassant(const glm::vec3& destination) const override {
            if (!Piece::isValidMove(destination)) return false;

            bool moved_towards_black = position.z - destination.z > 0.0f;
            bool is_white = !!color;
            bool moved_diagonal = fabs(destination.x - position.x) == 1.0f && fabs(position.z - destination.z) == 1.0f;
            bool unblocked = !piece_at(destination);

            return moved_diagonal && is_white == moved_towards_black && unblocked;
        }

        bool tryingToPromote(const glm::vec3& destination) const override {
            if (!Piece::isValidMove(destination)) return false;

            bool is_white = !!color;
            float final_square_z = is_white ? -3.5f : 3.5f;

            return destination.z == final_square_z;
        }
    };

    auto static getStructure(Pawn& obj) {
        return std::tie(obj.position, obj.model_name, obj.color, obj.board_id, obj.has_moved, obj.moved_count, obj.last_moved_position, obj.last_moved_turn);
    };

}
