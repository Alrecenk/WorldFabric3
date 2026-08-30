#pragma once
#include "Piece.h"

namespace Chess {

    class King :
        public Piece
    {
    public:

        King(const glm::vec3& position, const int64_t& board_id, const Piece::COLOR& color)
            : Piece(position, board_id, color, std::string("king") + (color ? "_white" : "_black")) { };

        King() = default;


        //This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
        // Just change the template parameter to match your class
        int getTypeId(Registry* r) const {
            return r->getIdForType<King>();
        }

        bool tryingToCastle(const glm::vec3& destination) const override {
            if (!Piece::isValidMove(destination)) return false;

            return fabs(destination.x - position.x) == 2.0f && fabs(position.z - destination.z) == 0;
        }

        bool isValidMove(const glm::vec3& destination) const override {
            if (!Piece::isValidMove(destination)) return false;

			if (fabs(position.x - destination.x) <= 1.0f && fabs(position.z - destination.z) <= 1.0f) return true;

            return false;
        }
    };

    auto static getStructure(King& obj) {
        return std::tie(obj.position, obj.model_name, obj.color, obj.board_id, obj.has_moved, obj.moved_count, obj.last_moved_position, obj.last_moved_turn);
    };

}
