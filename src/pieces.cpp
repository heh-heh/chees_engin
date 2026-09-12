#include "vector.cpp"
#include "linked_list.cpp"

#ifndef CHESS_PIECES_H
#define CHESS_PIECES_H

class Board;

class pieces {
public:
    // color: -1 white, 1 black
    // type: 1 pawn, 2 knight, 3 bishop, 4 rook, 5 queen, 6 king
    pieces(int piece_color, int piece_type, vector start_position)
        : color(piece_color), type(piece_type), nowposition(start_position) {};

    bool move(int x, int y, const Board* board = nullptr) {
        vector destination{x, y};

        if (!is_valid_move(destination, board)) {
            return false;
        }

        pathlist.clear();
        add_path(destination);
        nowposition = destination;
        ++turncount;
        return true;
    }

    bool can_move_to(const vector& destination, const Board* board = nullptr) const {
        if (!is_on_board(destination)) {
            return false;
        }

        if (board != nullptr && board->is_ally_at(destination, color)) {
            return false;
        }

        if (board != nullptr && !board->is_path_clear(nowposition, destination, type)) {
            return false;
        }

        return is_valid_move(destination, board);
    }

    bool is_publicly_valid_destination(const vector& destination, const Board* board = nullptr) const {
        return can_move_to(destination, board);
    }

    bool is_alive() const {
        return alive;
    }

    void set_alive(bool state) {
        alive = state;
    }

    bool is_enemy(const Board* board, const vector& destination) const {
        return board != nullptr && board->has_piece(destination) &&
               board->get_piece_color_at(destination) == -color;
    }

    bool is_ally(const Board* board, const vector& destination) const {
        return board != nullptr && board->has_piece(destination) &&
               board->get_piece_color_at(destination) == color;
    }

    bool is_in_check(const Board* board) const {
        if (board == nullptr || type != 6) {
            return false;
        }

        return board->is_square_attacked(nowposition, -color);
    }

    bool is_checkmate(const Board* board) const {
        if (board == nullptr || type != 6) {
            return false;
        }

        if (!is_in_check(board)) {
            return false;
        }

        return true;
    }

    vector position() const {
        return nowposition;
    }

    int getcolor() const {
        return color;
    }

    const linked_list& path() const {
        return pathlist;
    }
    int gettype() const{
        return type;
    }
    bool is_check(const Board* board, const vector& king_position) const {
        if (board == nullptr) {
            return false;
        }

        return board->is_square_attacked(king_position, -color);
    }
    bool is_checkmate(const Board* board, const vector& king_position) const {
        if (!is_check(board, king_position)) {
            return false;
        }

        return !board->is_square_attacked(king_position, -color);
    }

private:
    int color = 0;
    int type = 0;
    int turncount = 0;
    bool alive = true;
    linked_list pathlist;
    vector nowposition{0, 0};

    bool is_valid_move(vector destination, const Board* board = nullptr) const {
        int dx = destination.x - nowposition.x;
        int dy = destination.y - nowposition.y;
        int distance_x = absolute(dx);
        int distance_y = absolute(dy);

        if (!is_on_board(destination) || (dx == 0 && dy == 0)) {
            return false;
        }

        if (board != nullptr && board->is_ally_at(destination, color)) {
            return false;
        }

        if (board != nullptr && !board->is_path_clear(nowposition, destination, type)) {
            return false;
        }

        switch (type) {// type: 1 pawn, 2 knight, 3 bishop, 4 rook, 5 queen, 6 king
            case 1: {
                int direction = color == 1 ? 1 : -1;

                if (dy == 0 && dx == direction) {
                    return true;
                }

                return dy == 0 && dx == direction * 2 && turncount == 0;
            }
            case 2:
                return  (distance_x == 2 && distance_y == 1) ||
                        (distance_x == 1 && distance_y == 2);
            case 3:
                return distance_x == distance_y;
            case 4:

                return dx == 0 || dy == 0;
            case 5:
                return distance_x == distance_y || dx == 0 || dy == 0;
            case 6:
                return distance_x <= 1 && distance_y <= 1;
            default:
                return false;
        }
    }

    static int absolute(int value) {
        return value < 0 ? -value : value;
    }

    static bool is_on_board(vector position) {
        return  position.x >= 0 && position.x < 8 &&
                position.y >= 0 && position.y < 8;
    }

    void add_path(vector destination) {
        int dx = destination.x - nowposition.x;
        int dy = destination.y - nowposition.y;
        int step_x = dx == 0 ? 0 : (dx > 0 ? 1 : -1);
        int step_y = dy == 0 ? 0 : (dy > 0 ? 1 : -1);
        int steps = absolute(dx) > absolute(dy) ? absolute(dx) : absolute(dy);

        for (int step = 1; step < steps; ++step) {
            pathlist.push_back(vector{
                nowposition.x + step_x * step,
                nowposition.y + step_y * step
            });
        }
    }
};

#endif