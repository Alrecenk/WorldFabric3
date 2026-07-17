#pragma once
#include "Piece.h"

namespace Chess {

    class Knight :
        public Piece
    {
    public:

        Knight(const glm::vec3& position, const int64_t& board_id, const Piece::COLOR& color)
            : Piece(position, board_id, color, std::string("knight") + (color ? "_white" : "_black")) {};

        Knight() = default;


        //This needs to be in every WorldObject to deduce types for serialziation templates from polymorphism
        // Just change the template parameter to match your class
        int getTypeId(Registry* r) const {
            return r->getIdForType<Knight>();
        }

        bool isValidMove(const glm::vec3& destination) const override {
            if (!Piece::isValidMove(destination)) return false;

            return fabs(position.x - destination.x) == 2 && fabs(position.z - destination.z) == 1
                || fabs(position.x - destination.x) == 1 && fabs(position.z - destination.z) == 2;
        }
    };

    auto static getStructure(Knight& obj) {
        return std::tie(obj.position, obj.model_name, obj.color, obj.board_id, obj.has_moved);
    };

}
