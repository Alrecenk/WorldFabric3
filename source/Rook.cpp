#include "Piece.h"
#include "Board.h"

#include "ChessApp.h"
#include <print>


namespace Chess {

    bool Rook::isValidMove(const glm::vec3& destination) const {
        if (!Piece::isValidMove(destination)) return false;

        return !blocked_by(destination) && moved_like_rook(destination);
    }

    void Rook::castle() {
        float new_x = position.x < 0 ? -0.5f : 1.5f;
        queue(board_id, time, &Board::setPiecePosition, position, glm::vec3(new_x, position.y, position.z));
    }
}
