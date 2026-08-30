#include "Piece.h"
#include "Board.h"

#include "ChessApp.h"
#include <print>


namespace Chess {
    bool Pawn::isValidMove(const glm::vec3& destination) const {
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
    bool Pawn::tryingToEnPassant(const glm::vec3& destination) const {
        WorldPlugin* world = getTool<WorldPlugin>();
        if (!Piece::isValidMove(destination)) return false;

        bool moved_towards_black = position.z - destination.z > 0.0f;
        bool is_white = !!color;
        bool moved_diagonal = fabs(destination.x - position.x) == 1.0f && fabs(position.z - destination.z) == 1.0f;
        bool unblocked = !piece_at(destination);
        auto enemy_pawn_pos = destination;
        enemy_pawn_pos.z += is_white ? 1 : -1;
        auto enemy_pawn = world->observe<Pawn>("chess", piece_at(enemy_pawn_pos));
        auto board = world->observeNearest<Board>("chess");

        bool enemy_pawn_capturable = enemy_pawn && fabs(enemy_pawn->position.z) == 0.5f && enemy_pawn->last_moved_turn == board->turn_count - 1 && fabs(enemy_pawn->last_moved_position.z) == 2.5f;

        return moved_diagonal && is_white == moved_towards_black && unblocked && enemy_pawn_capturable;
    }

    bool Pawn::tryingToPromote(const glm::vec3& destination) const {
        if (!Piece::isValidMove(destination)) return false;
        if (!isValidMove(destination)) return false;

        bool is_white = !!color;
        float final_square_z = is_white ? -3.5f : 3.5f;

        return destination.z == final_square_z;
    }
}
