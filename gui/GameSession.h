#pragma once

#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../src/game/bord.cpp"
#include "../src/manager/Director.cpp"
#include "../src/object/pieces.cpp"

using BoardState = std::array<std::array<int, 8>, 8>;

enum class MoveOutcome {
    Selected,
    Deselected,
    DifferentSelected,
    NoLegalMoves,
    NotYourTurn,
    IllegalMove,
    Moved,
    NoActivePiece
};

struct ClickResult {
    MoveOutcome outcome = MoveOutcome::NoActivePiece;
    int fromRow = -1;
    int fromCol = -1;
    int toRow = -1;
    int toCol = -1;
    int movingColor = 0;
    bool checkmate = false;
    int winnerColor = 0;
};

// GameSession wraps the console engine (Board/Director/pieces) for GUI use.
class GameSession {
public:
    GameSession();

    void reset(bool multiplayer);

    int pieceAt(int row, int col) const { return board_[row][col]; }

    bool isMultiplayer() const { return is_multiplayer_; }

    bool isGameOver() const { return is_game_over_; }
    int winnerColor() const { return winner_color_; }

    int turn() const { return turn_; }
    int currentColor() const { return current_color_; }

    int selectedRow() const { return selected_row_; }
    int selectedCol() const { return selected_col_; }
    bool isLegalTarget(int row, int col) const;

    const std::string& statusMessage() const { return status_message_; }
    void setStatusMessage(const std::string& message) { status_message_ = message; }

    std::chrono::milliseconds turnElapsed() const { return engine_director_.get_turn_time(); }
    int evaluateBoard() const { return engine_director_.evaluate_board(engine_board_); }
    bool isKingInCheck(int color) const { return engine_director_.is_king_in_check(engine_board_, color); }
    bool canCastleKingside(int color) const { return engine_board_.can_castle(color, true); }
    bool canCastleQueenside(int color) const { return engine_board_.can_castle(color, false); }
    std::pair<int, int> kingPosition(int color) const;

    // Handles a board click. localPlayerColor is only enforced in multiplayer mode.
    ClickResult handleClick(int row, int col, int localPlayerColor);
    bool applyRemoteMove(int fromRow, int fromCol, int toRow, int toCol, int color);

private:
    void syncBoardFromPieces();
    void syncEngineBoard();
    std::vector<std::pair<int, int>> collectLegalMoves(pieces* piece) const;
    pieces* findPieceAt(int row, int col) const;
    static bool wouldLeaveKingInCheck(const Board& board, const pieces& movingPiece, const vector& destination);

    Board engine_board_;
    std::vector<std::unique_ptr<pieces>> engine_pieces_;
    Director engine_director_;
    BoardState board_{};

    int turn_ = 1;
    int current_color_ = 1;
    int selected_row_ = -1;
    int selected_col_ = -1;
    std::vector<std::pair<int, int>> legal_moves_;
    std::string status_message_ = "Select a piece to move.";
    bool is_multiplayer_ = false;
    bool is_game_over_ = false;
    int winner_color_ = 0;
};
