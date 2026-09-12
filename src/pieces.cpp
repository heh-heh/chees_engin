#include "vector.cpp"
#include "linked_list.cpp"

#ifndef CHESS_PIECES_H
#define CHESS_PIECES_H

class Board;

class pieces {
public:
    // color: -1 white, 1 black
    // type: 1 pawn, 2 knight, 3 bishop, 4 rook, 5 queen, 6 king
    // pieces는 각 기물의 색상, 종류, 현재 위치, 생존 상태를 가진다.
    pieces(int piece_color, int piece_type, vector start_position)
        : color(piece_color), type(piece_type), nowposition(start_position) {};

    // move(): 실제 이동 시도. 우선 is_valid_move()가 통과해야 하고,
    // 통과하면 경로 리스트(pathlist)와 nowposition을 갱신한다.
    // board는 경로 차단/공격자 좌표 파악에 필요한 상태 점검용 인자다.
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

    // can_move_to(): 화면색으로 보이는 후보 칸을 실제 이동 가능 여부로 판별한다.
    // 보드와 함께 쓰면 아군 기물, 경로 차단, 슬라이딩 기물 장애물을 처리한다.
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

    // is_publicly_valid_destination(): 화면에서 표시할 후보 좌표의 공개용 판정 래퍼.
    bool is_publicly_valid_destination(const vector& destination, const Board* board = nullptr) const {
        return can_move_to(destination, board);
    }

    // alive 상태는 잡힌 기물을 보드 조회 대상에서 제외하는 데 사용한다.
    bool is_alive() const {
        return alive;
    }

    // 잡힌 기물은 남겨두되 생존 여부를 false로 바꾼다.
    void set_alive(bool state) {
        alive = state;
    }

    // 보드의 특정 좌표가 상대 색 기물인지 판정하는 보조 함수.
    bool is_enemy(const Board* board, const vector& destination) const {
        return board != nullptr && board->has_piece(destination) &&
               board->get_piece_color_at(destination) == -color;
    }

    // 보드의 특정 좌표가 아군 기물인지 판정하는 보조 함수.
    bool is_ally(const Board* board, const vector& destination) const {
        return board != nullptr && board->has_piece(destination) &&
               board->get_piece_color_at(destination) == color;
    }

    // 지정한 보드 상태에서 현재 기물 객체의 왕(king)이 체크당하는지 판정한다.
    bool is_in_check(const Board* board) const {
        if (board == nullptr || type != 6) {
            return false;
        }

        return board->is_square_attacked(nowposition, -color);
    }

    // 체크 상태에서 추가 이동이 가능하지 않은지 메이트의 기본 조건을 판정한다.
    bool is_checkmate(const Board* board) const {
        if (board == nullptr || type != 6) {
            return false;
        }

        if (!is_in_check(board)) {
            return false;
        }

        return true;
    }

    // 현재 좌표를 외부에서 조회할 때 쓰는 접근자.
    vector position() const {
        return nowposition;
    }

    // 현재 기물의 색을 외부에서 조회할 때 쓰는 접근자.
    int getcolor() const {
        return color;
    }

    // 경로 계산 결과가 저장된 연결 리스트를 외부에서 읽기 위한 접근자.
    const linked_list& path() const {
        return pathlist;
    }

    // 기물 종류 번호를 외부에서 읽기 위한 접근자.
    int gettype() const{
        return type;
    }

    // 체크 여부를 좌표 인자 형태로 판정하는 보조 함수다.
    bool is_check(const Board* board, const vector& king_position) const {
        if (board == nullptr) {
            return false;
        }

        return board->is_square_attacked(king_position, -color);
    }

    // 메이트 판단의 시작점이 되는 함수. 아직 전체 이동 후보를 한 번에 계산하지는 않는다.
    bool is_checkmate(const Board* board, const vector& king_position) const {
        if (!is_check(board, king_position)) {
            return false;
        }

        return !board->is_square_attacked(king_position, -color);
    }

private:
    // color: 1 흰색, -1 검은색.
    int color = 0;

    // type: 1 pawn, 2 knight, 3 bishop, 4 rook, 5 queen, 6 king.
    int type = 0;

    // 이동 횟수는 폰의 두 칸 전진 여부처럼 초기 상태 판단에 사용한다.
    int turncount = 0;

    // 기물 생존 상태. 잡힌 기물은 false로 두고 보드 조회에서 걸러낸다.
    bool alive = true;

    // 이동 경로를 연결 리스트로 저장해 출력이나 디버깅에 활용한다.
    linked_list pathlist;

    // 현재 기물의 좌표값.
    vector nowposition{0, 0};

    // is_valid_move(): 기물의 종류에 따라 실제 이동 규칙을 판단한다.
    // board는 아군/적군 구분과 경로 차단 여부를 확인하는 데 사용한다.
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

    // 절댓값 유틸. 좌표 거리 계산에서 공통으로 쓰는 값이다.
    static int absolute(int value) {
        return value < 0 ? -value : value;
    }

    // 좌표 범위 안에 있는지 확인한다.
    static bool is_on_board(vector position) {
        return  position.x >= 0 && position.x < 8 &&
                position.y >= 0 && position.y < 8;
    }

    // add_path(): 슬라이딩 기물의 경로 좌표를 linked_list에 쌓는 함수다.
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