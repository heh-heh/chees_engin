#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <vector>
#include "../datas/vector.cpp"

#ifndef CHESS_BOARD_H
#define CHESS_BOARD_H

class Board {
    public:
        Board() = default;

        void printbord(const vector* selected = nullptr,
                        const std::vector<vector>* legal_moves = nullptr,
                        const std::vector<vector>* blocked_moves = nullptr) {
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    bool is_selected = selected != nullptr &&
                                       selected->x == x && selected->y == y;
                    bool is_legal_move = legal_moves != nullptr &&
                                         std::any_of(legal_moves->begin(), legal_moves->end(),
                                            [x, y](const vector& move) {
                                                return move.x == x && move.y == y;
                                            });
                    bool is_blocked_move = blocked_moves != nullptr &&
                                           std::any_of(blocked_moves->begin(), blocked_moves->end(),
                                            [x, y](const vector& move) {
                                                return move.x == x && move.y == y;
                                            });

                    if (is_selected) {
                        std::cout << "\033[43;30m";
                    } else if (is_legal_move) {
                        std::cout << "\033[44;30m";
                    } else if (is_blocked_move) {
                        std::cout << "\033[41;30m";
                    }

                    std::cout << std::setw(2) << map[y][x];

                    if (is_selected || is_legal_move || is_blocked_move) {
                        std::cout << "\033[0m";
                    }
                    std::cout << "  ";
                }
                std::cout << std::endl;
                std::cout << std::endl;
            }
        }

        void move_piece(vector from, vector to) {
            const int moving_piece = map[from.x][from.y];
            const int captured_value = map[to.x][to.y];
            const int moving_color = moving_piece > 0 ? 1 : -1;

            if (moving_piece == 0) {
                return;
            }

            if (std::abs(moving_piece) == 4) {
                disable_rook_castling_rights(from, moving_color);
            }
            if (std::abs(moving_piece) == 6) {
                disable_king_castling_rights(moving_color);
            }
            if (captured_value != 0 && std::abs(captured_value) == 4) {
                disable_rook_castling_rights(to, captured_value > 0 ? 1 : -1);
            }

            map[to.x][to.y] = moving_piece;
            map[from.x][from.y] = 0;
            update_en_passant_state(from, to, moving_piece);
        }

        void remove_piece_at(vector position) {
            const int captured_value = map[position.x][position.y];
            if (captured_value != 0 && std::abs(captured_value) == 4) {
                disable_rook_castling_rights(position, captured_value > 0 ? 1 : -1);
            }
            if (captured_value != 0 && std::abs(captured_value) == 6) {
                disable_king_castling_rights(captured_value > 0 ? 1 : -1);
            }
            map[position.x][position.y] = 0;
        }

        void clear_board() {
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    map[x][y] = 0;
                }
            }
            en_passant_target = {-1, -1};
            white_can_castle_kingside = true;
            white_can_castle_queenside = true;
            black_can_castle_kingside = true;
            black_can_castle_queenside = true;
        }

        void set_piece_at(const vector& position, int piece_value) {
            if (position.x >= 0 && position.x < 8 && position.y >= 0 && position.y < 8) {
                map[position.x][position.y] = piece_value;
            }
        }

        int get_piece_value_at(const vector& position) const {
            if (position.x < 0 || position.x >= 8 || position.y < 0 || position.y >= 8) {
                return 0;
            }
            return map[position.x][position.y];
        }

        bool has_piece(const vector& position) const {
            return map[position.x][position.y] != 0;
        }

        int get_piece_color_at(const vector& position) const {
            int value = map[position.x][position.y];
            if (value == 0) {
                return 0;
            }
            return value > 0 ? 1 : -1;
        }

        int get_piece_type_at(const vector& position) const {
            int value = map[position.x][position.y];
            if (value == 0) {
                return 0;
            }
            return std::abs(value);
        }

        bool is_enemy_at(const vector& position, int own_color) const {
            int occupant_color = get_piece_color_at(position);
            return occupant_color != 0 && occupant_color != own_color;
        }

        bool is_ally_at(const vector& position, int own_color) const {
            int occupant_color = get_piece_color_at(position);
            return occupant_color == own_color;
        }

        bool is_path_clear(const vector& from, const vector& to, int piece_type) const {
            int dx = to.x - from.x;
            int dy = to.y - from.y;

            bool is_sliding_piece = piece_type == 3 || piece_type == 4 || piece_type == 5;
            if (!is_sliding_piece) {
                return true;
            }

            bool is_straight = dx == 0 || dy == 0;
            bool is_diagonal = std::abs(dx) == std::abs(dy);
            if (!(is_straight || is_diagonal)) {
                return true;
            }

            int step_x = dx == 0 ? 0 : (dx > 0 ? 1 : -1);
            int step_y = dy == 0 ? 0 : (dy > 0 ? 1 : -1);
            int steps = std::max(std::abs(dx), std::abs(dy));

            for (int step = 1; step < steps; ++step) {
                vector next_position{
                    from.x + step_x * step,
                    from.y + step_y * step
                };

                if (map[next_position.x][next_position.y] != 0) {
                    return false;
                }
            }

            return true;
        }

        bool is_square_attacked(const vector& target, int attacker_color) const {
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    int value = map[x][y];
                    if (value == 0) {
                        continue;
                    }

                    int piece_color = value > 0 ? 1 : -1;
                    if (piece_color != attacker_color) {
                        continue;
                    }

                    int piece_type = std::abs(value);
                    vector from{x, y};
                    int dx = target.x - from.x;
                    int dy = target.y - from.y;
                    int distance_x = std::abs(dx);
                    int distance_y = std::abs(dy);

                    switch (piece_type) {
                        case 1:
                            if (distance_x == 1 && dy == (attacker_color == 1 ? -1 : 1)) {
                                return true;
                            }
                            break;
                        case 2:
                            if ((distance_x == 2 && distance_y == 1) ||
                                (distance_x == 1 && distance_y == 2)) {
                                return true;
                            }
                            break;
                        case 3:
                            if (distance_x == distance_y && is_line_clear(from, target)) {
                                return true;
                            }
                            break;
                        case 4:
                            if ((dx == 0 || dy == 0) && is_line_clear(from, target)) {
                                return true;
                            }
                            break;
                        case 5:
                            if (((distance_x == distance_y) || (dx == 0 || dy == 0)) && is_line_clear(from, target)) {
                                return true;
                            }
                            break;
                        case 6:
                            if (distance_x <= 1 && distance_y <= 1) {
                                return true;
                            }
                            break;
                    }
                }
            }

            return false;
        }

        bool is_en_passant_target(const vector& target, int moving_color) const {
            if (en_passant_target.x < 0 || en_passant_target.y < 0) {
                return false;
            }
            return en_passant_target.x == target.x && en_passant_target.y == target.y && moving_color != 0;
        }

        bool is_line_clear(const vector& from, const vector& target) const {
            int dx = target.x - from.x;
            int dy = target.y - from.y;
            if (dx == 0 && dy == 0) {
                return true;
            }

            int step_x = dx == 0 ? 0 : (dx > 0 ? 1 : -1);
            int step_y = dy == 0 ? 0 : (dy > 0 ? 1 : -1);
            int steps = std::max(std::abs(dx), std::abs(dy));

            for (int step = 1; step < steps; ++step) {
                vector pos{from.x + step_x * step, from.y + step_y * step};
                if (map[pos.x][pos.y] != 0) {
                    return false;
                }
            }
            return true;
        }

        bool is_king_in_check(int color) const {
            vector king_position{-1, -1};
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    const vector position{x, y};
                    if (get_piece_value_at(position) == color * 6) {
                        king_position = position;
                        break;
                    }
                }
                if (king_position.x >= 0) {
                    break;
                }
            }

            if (king_position.x < 0) {
                return false;
            }

            return is_square_attacked(king_position, -color);
        }

        bool can_castle(int color, bool kingside) const {
            const int home_row = color == 1 ? 7 : 0;
            const int king_value = color * 6;
            const int rook_value = color * 4;
            const int king_file = 4;
            const int rook_file = kingside ? 7 : 0;
            const int safe_file = kingside ? 6 : 2;

            if (home_row == 7 && ((color == 1 && !white_can_castle_kingside) || (color == 1 && !white_can_castle_queenside))) {
                if (kingside && !white_can_castle_kingside) return false;
                if (!kingside && !white_can_castle_queenside) return false;
            }
            if (home_row == 0 && ((color == -1 && !black_can_castle_kingside) || (color == -1 && !black_can_castle_queenside))) {
                if (kingside && !black_can_castle_kingside) return false;
                if (!kingside && !black_can_castle_queenside) return false;
            }

            if (get_piece_value_at({king_file, home_row}) != king_value) {
                return false;
            }
            if (get_piece_value_at({rook_file, home_row}) != rook_value) {
                return false;
            }

            const int path_start = kingside ? 5 : 1;
            const int path_end = kingside ? 6 : 3;
            for (int x = path_start; x <= path_end; ++x) {
                if (x == 4) continue;
                if (has_piece({x, home_row})) {
                    return false;
                }
            }

            if (is_king_in_check(color)) {
                return false;
            }

            for (int x = path_start; x <= path_end; ++x) {
                if (is_square_attacked({x, home_row}, -color)) {
                    return false;
                }
            }

            return true;
        }

        bool is_checkmate_for(int color) const {
            if (!is_king_in_check(color)) {
                return false;
            }

            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    const vector from{x, y};
                    const int piece_value = get_piece_value_at(from);
                    if (piece_value == 0 || get_piece_color_at(from) != color) {
                        continue;
                    }

                    for (int target_y = 0; target_y < 8; ++target_y) {
                        for (int target_x = 0; target_x < 8; ++target_x) {
                            const vector target{target_x, target_y};
                            if (!can_piece_move_to(from, target, piece_value)) {
                                continue;
                            }

                            Board candidate = *this;
                            const int piece_type = std::abs(piece_value);
                            if (piece_type == 1 && std::abs(target.x - from.x) == 1 && !candidate.has_piece(target) && candidate.is_en_passant_target(target, color)) {
                                const vector captured = {target.x, target.y - (color == 1 ? -1 : 1)};
                                candidate.remove_piece_at(captured);
                            }
                            candidate.move_piece(from, target);
                            if (!candidate.is_king_in_check(color)) {
                                return false;
                            }
                        }
                    }
                }
            }

            return true;
        }

    private:
        int map[8][8] = {
            {-4,-2,-3,-5,-6,-3,-2,-4},
            {-1,-1,-1,-1,-1,-1,-1,-1},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {1, 1, 1, 1, 1, 1, 1, 1},
            {4, 2, 3, 5, 6, 3, 2, 4}
        };

        vector en_passant_target{-1, -1};
        bool white_can_castle_kingside = true;
        bool white_can_castle_queenside = true;
        bool black_can_castle_kingside = true;
        bool black_can_castle_queenside = true;

        void update_en_passant_state(const vector& from, const vector& to, int moving_piece) {
            const int piece_type = std::abs(moving_piece);
            const int piece_color = moving_piece > 0 ? 1 : -1;
            en_passant_target = {-1, -1};

            if (piece_type == 1 && std::abs(to.y - from.y) == 2) {
                const int direction = piece_color == 1 ? -1 : 1;
                en_passant_target = {to.x, to.y - direction};
            }
        }

        void disable_king_castling_rights(int color) {
            if (color == 1) {
                white_can_castle_kingside = false;
                white_can_castle_queenside = false;
            } else if (color == -1) {
                black_can_castle_kingside = false;
                black_can_castle_queenside = false;
            }
        }

        void disable_rook_castling_rights(const vector& position, int color) {
            if (position.x == 0 && position.y == (color == 1 ? 7 : 0)) {
                if (color == 1) white_can_castle_queenside = false;
                else black_can_castle_queenside = false;
            }
            if (position.x == 7 && position.y == (color == 1 ? 7 : 0)) {
                if (color == 1) white_can_castle_kingside = false;
                else black_can_castle_kingside = false;
            }
        }

        bool can_piece_move_to(const vector& from, const vector& target, int piece_value) const {
            if (from.x < 0 || from.x >= 8 || from.y < 0 || from.y >= 8 ||
                target.x < 0 || target.x >= 8 || target.y < 0 || target.y >= 8 ||
                from.x == target.x && from.y == target.y) {
                return false;
            }

            const int piece_color = piece_value > 0 ? 1 : -1;
            const int piece_type = std::abs(piece_value);
            if (is_ally_at(target, piece_color)) {
                return false;
            }

            const int dx = target.x - from.x;
            const int dy = target.y - from.y;
            const int distance_x = std::abs(dx);
            const int distance_y = std::abs(dy);

            switch (piece_type) {
                case 1: {
                    const int direction = piece_color == 1 ? -1 : 1;
                    if (dx == 0 && dy == direction && !has_piece(target)) {
                        return true;
                    }
                    if (distance_x == 1 && dy == direction) {
                        if (has_piece(target) && is_enemy_at(target, piece_color)) {
                            return true;
                        }
                        if (!has_piece(target) && is_en_passant_target(target, piece_color)) {
                            return true;
                        }
                    }
                    return dx == 0 && dy == direction * 2 && from.y == (piece_color == 1 ? 6 : 1) &&
                           !has_piece({from.x, from.y + direction}) && !has_piece(target);
                }
                case 2:
                    return (distance_x == 2 && distance_y == 1) || (distance_x == 1 && distance_y == 2);
                case 3:
                    return distance_x == distance_y && is_path_clear(from, target, piece_type);
                case 4:
                    return (dx == 0 || dy == 0) && is_path_clear(from, target, piece_type);
                case 5:
                    return ((distance_x == distance_y) || (dx == 0 || dy == 0)) && is_path_clear(from, target, piece_type);
                case 6: {
                    const bool castling_move = (std::abs(target.x - from.x) == 2 && target.y == from.y);
                    if (castling_move) {
                        if (from.x != 4) {
                            return false;
                        }
                        const int color = piece_value > 0 ? 1 : -1;
                        const bool kingside = target.x > from.x;
                        return can_castle(color, kingside);
                    }
                    return distance_x <= 1 && distance_y <= 1;
                }
                default:
                    return false;
            }
        }
};

#endif