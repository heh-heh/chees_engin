#include "GameSession.h"

#include <algorithm>
#include <cmath>

namespace {

vector engine_pos_from_ui(int row, int col) {
    return {col, row};
}

void initialize_engine_pieces(std::vector<std::unique_ptr<pieces>>& piece_list) {
    piece_list.clear();
    const int backrank[8] = {4, 2, 3, 5, 6, 3, 2, 4};

    for (int x = 0; x < 8; ++x) {
        piece_list.push_back(std::make_unique<pieces>(1, backrank[x], vector{x, 7}));
        piece_list.push_back(std::make_unique<pieces>(1, 1, vector{x, 6}));
        piece_list.push_back(std::make_unique<pieces>(-1, 1, vector{x, 1}));
        piece_list.push_back(std::make_unique<pieces>(-1, backrank[x], vector{x, 0}));
    }
}

}  // namespace

GameSession::GameSession() {
    reset(false);
}

void GameSession::reset(bool multiplayer) {
    is_multiplayer_ = multiplayer;
    initialize_engine_pieces(engine_pieces_);
    engine_board_ = Board{};
    syncEngineBoard();
    engine_director_ = Director{};
    syncBoardFromPieces();
    turn_ = 1;
    current_color_ = engine_director_.get_current_color();
    selected_row_ = -1;
    selected_col_ = -1;
    legal_moves_.clear();
    is_game_over_ = false;
    winner_color_ = 0;
    status_message_ = is_multiplayer_ ? "Multiplayer match ready." : "Single-player match ready.";
}

bool GameSession::isLegalTarget(int row, int col) const {
    return std::find(legal_moves_.begin(), legal_moves_.end(), std::make_pair(row, col)) != legal_moves_.end();
}

std::pair<int, int> GameSession::kingPosition(int color) const {
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const int value = board_[row][col];
            if (value != 0 && std::abs(value) == 6 && (value > 0) == (color > 0)) {
                return {row, col};
            }
        }
    }
    return {-1, -1};
}

void GameSession::syncBoardFromPieces() {
    BoardState state{};
    for (const auto& piece : engine_pieces_) {
        if (!piece->is_alive()) {
            continue;
        }
        const vector position = piece->position();
        state[position.y][position.x] = piece->getcolor() * piece->gettype();
    }
    board_ = state;
}

void GameSession::syncEngineBoard() {
    engine_board_.clear_board();
    for (const auto& piece : engine_pieces_) {
        if (!piece->is_alive()) {
            continue;
        }
        const vector position = piece->position();
        engine_board_.set_piece_at(position, piece->getcolor() * piece->gettype());
    }
}

pieces* GameSession::findPieceAt(int row, int col) const {
    const vector engine_pos = engine_pos_from_ui(row, col);
    for (const auto& piece : engine_pieces_) {
        if (!piece->is_alive()) {
            continue;
        }
        const vector position = piece->position();
        if (position.x == engine_pos.x && position.y == engine_pos.y) {
            return piece.get();
        }
    }
    return nullptr;
}

bool GameSession::wouldLeaveKingInCheck(const Board& board, const pieces& movingPiece, const vector& destination) {
    Board candidate = board;
    const vector from = movingPiece.position();
    const int moving_color = movingPiece.getcolor();

    const bool en_passant_capture = movingPiece.gettype() == 1 &&
        std::abs(destination.x - from.x) == 1 &&
        !candidate.has_piece(destination) &&
        candidate.is_en_passant_target(destination, moving_color);

    if (en_passant_capture) {
        const vector captured_position{destination.x, destination.y - (moving_color == 1 ? -1 : 1)};
        candidate.remove_piece_at(captured_position);
    }

    if (movingPiece.gettype() == 6 && std::abs(destination.x - from.x) == 2) {
        const int rook_from_x = destination.x > from.x ? 7 : 0;
        const int rook_to_x = destination.x > from.x ? 5 : 3;
        candidate.move_piece({rook_from_x, from.y}, {rook_to_x, from.y});
    }

    candidate.move_piece(from, destination);
    return candidate.is_king_in_check(moving_color);
}

std::vector<std::pair<int, int>> GameSession::collectLegalMoves(pieces* piece) const {
    std::vector<std::pair<int, int>> moves;
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const vector destination = engine_pos_from_ui(row, col);
            if (!piece->can_move_to(destination, &engine_board_)) {
                continue;
            }
            if (wouldLeaveKingInCheck(engine_board_, *piece, destination)) {
                continue;
            }
            moves.emplace_back(row, col);
        }
    }
    return moves;
}

ClickResult GameSession::handleClick(int row, int col, int localPlayerColor) {
    ClickResult result;

    syncEngineBoard();
    syncBoardFromPieces();

    const auto select_piece = [&](pieces* clicked_piece, const char* selectedMessage, const char* differentMessage) {
        if (clicked_piece != nullptr && engine_director_.can_select(*clicked_piece) &&
            (!is_multiplayer_ || clicked_piece->getcolor() == localPlayerColor)) {
            selected_row_ = row;
            selected_col_ = col;
            legal_moves_ = collectLegalMoves(clicked_piece);
            status_message_ = legal_moves_.empty() ? "No legal moves for this piece." : selectedMessage;
            result.outcome = legal_moves_.empty() ? MoveOutcome::NoLegalMoves : MoveOutcome::Selected;
        } else if (is_multiplayer_ && clicked_piece != nullptr && clicked_piece->getcolor() != localPlayerColor) {
            status_message_ = "It is not your turn to move that piece.";
            result.outcome = MoveOutcome::NotYourTurn;
        } else {
            status_message_ = differentMessage;
            result.outcome = MoveOutcome::IllegalMove;
        }
    };

    pieces* clicked_piece = findPieceAt(row, col);

    if (selected_row_ == -1 && selected_col_ == -1) {
        select_piece(clicked_piece, "Piece selected. Click a highlighted square.", "It is not your turn to move that piece.");
        return result;
    }

    if (selected_row_ == row && selected_col_ == col) {
        selected_row_ = -1;
        selected_col_ = -1;
        legal_moves_.clear();
        status_message_ = "Selection cleared.";
        result.outcome = MoveOutcome::Deselected;
        return result;
    }

    if (isLegalTarget(row, col)) {
        pieces* moving_piece = findPieceAt(selected_row_, selected_col_);
        if (moving_piece == nullptr) {
            status_message_ = "No active piece selected.";
            result.outcome = MoveOutcome::NoActivePiece;
            return result;
        }

        const vector from = engine_pos_from_ui(selected_row_, selected_col_);
        const vector to = engine_pos_from_ui(row, col);
        pieces* captured_piece = findPieceAt(row, col);

        const bool en_passant_capture = moving_piece->gettype() == 1 &&
            std::abs(to.x - from.x) == 1 &&
            !engine_board_.has_piece(to) &&
            engine_board_.is_en_passant_target(to, moving_piece->getcolor());

        if (!engine_director_.move_piece(*moving_piece, to.x, to.y, &engine_board_)) {
            status_message_ = "Illegal move for this piece.";
            result.outcome = MoveOutcome::IllegalMove;
            return result;
        }

        if (en_passant_capture) {
            const vector captured_position{to.x, to.y - (moving_piece->getcolor() == 1 ? -1 : 1)};
            for (auto& piece : engine_pieces_) {
                if (piece->is_alive() && piece->position().x == captured_position.x && piece->position().y == captured_position.y) {
                    piece->set_alive(false);
                    break;
                }
            }
            engine_board_.remove_piece_at(captured_position);
        }

        if (captured_piece != nullptr && captured_piece != moving_piece) {
            captured_piece->set_alive(false);
            engine_board_.remove_piece_at(to);
        }
        engine_board_.move_piece(from, to);
        syncEngineBoard();

        result.fromRow = selected_row_;
        result.fromCol = selected_col_;
        result.toRow = row;
        result.toCol = col;
        result.movingColor = moving_piece->getcolor();

        selected_row_ = -1;
        selected_col_ = -1;
        legal_moves_.clear();
        syncBoardFromPieces();
        current_color_ = engine_director_.get_current_color();
        turn_ = static_cast<int>(engine_director_.get_turn_count()) + 1;

        const int opponent_color = -current_color_;
        const bool opponent_in_check = engine_director_.is_king_in_check(engine_board_, opponent_color);
        const bool opponent_checkmate = opponent_in_check && engine_director_.is_checkmate_for(engine_board_, opponent_color);
        if (opponent_checkmate) {
            is_game_over_ = true;
            winner_color_ = -opponent_color;
            status_message_ = "Checkmate! " + std::string(winner_color_ == 1 ? "White" : "Black") + " wins.";
            result.checkmate = true;
            result.winnerColor = winner_color_;
        } else {
            status_message_ = "Move applied.";
        }

        result.outcome = MoveOutcome::Moved;
        return result;
    }

    select_piece(clicked_piece, "Different piece selected.", "Illegal move for this piece.");
    if (result.outcome == MoveOutcome::Selected) {
        result.outcome = MoveOutcome::DifferentSelected;
    }
    return result;
}

bool GameSession::applyRemoteMove(int fromRow, int fromCol, int toRow, int toCol, int color) {
    pieces* remote_piece = findPieceAt(fromRow, fromCol);
    if (remote_piece == nullptr || remote_piece->getcolor() != color) {
        return false;
    }

    const vector from = engine_pos_from_ui(fromRow, fromCol);
    const vector to = engine_pos_from_ui(toRow, toCol);
    pieces* captured_piece = findPieceAt(toRow, toCol);
    if (!engine_director_.move_piece(*remote_piece, to.x, to.y, &engine_board_)) {
        return false;
    }

    if (captured_piece != nullptr && captured_piece != remote_piece) {
        captured_piece->set_alive(false);
        engine_board_.remove_piece_at(to);
    }
    engine_board_.move_piece(from, to);
    syncEngineBoard();
    selected_row_ = -1;
    selected_col_ = -1;
    legal_moves_.clear();
    syncBoardFromPieces();
    current_color_ = engine_director_.get_current_color();
    turn_ = static_cast<int>(engine_director_.get_turn_count()) + 1;
    status_message_ = "Opponent moved. Your turn.";
    return true;
}
