#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <vector>
#include "../datas/vector.cpp"

#ifndef CHESS_BOARD_H
#define CHESS_BOARD_H

class Board {
    public :
        Board(){
        }
        void printbord(const vector* selected = nullptr,
                        const std::vector<vector>* legal_moves = nullptr,
                        const std::vector<vector>* blocked_moves = nullptr){
            for(int y=0;y<8;y++){
                for(int x=0;x<8;x++){
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

                    std::cout << std::setw(2) << map[x][y];

                    if (is_selected || is_legal_move || is_blocked_move) {
                        std::cout << "\033[0m";
                    }
                    std::cout << "  ";
                }
                std::cout << std::endl;
                std::cout << std::endl;
            }
        }
        // 보드 배열의 숫자는 기물 타입과 색을 함께 표현하는 값이므로,
        // 좌표 이동은 칸의 값을 출발 칸에서 목적지 칸으로 복사하는 형태다.
        void move_piece(vector from, vector to){
            map[to.x][to.y] = map[from.x][from.y];
            map[from.x][from.y] = 0;
        }

        // 잡힌 기물은 보드 안쪽에 남아 있지 않도록 해당 위치를 비운다.
        void remove_piece_at(vector position){
            map[position.x][position.y] = 0;
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
                            if (dy == 1 && distance_x == 1) {
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
                            if (distance_x == distance_y) {
                                return true;
                            }
                            break;
                        case 4:
                            if (dx == 0 || dy == 0) {
                                return true;
                            }
                            break;
                        case 5:
                            if (distance_x == distance_y || dx == 0 || dy == 0) {
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
    private :
        int map[8][8]={
            {4, 2, 3, 5, 6, 3, 2, 4},
            {1, 1, 1, 1, 1, 1, 1, 1},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {-1,-1,-1,-1,-1,-1,-1,-1},
            {-4,-2,-3,-5,-6,-3,-2,-4}
        };
};

#endif