#include <iostream>
#include <memory>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>
#include "bord.cpp"
#include "Director.cpp"
#include "pieces.cpp"

class gamemanager{
    public:
        std::atomic<bool> endgame{false};
        void startgame(){
            const int backrank[8] = {4, 2, 3, 5, 6, 3, 2, 4};
            pieces2.clear();
            pieces2.reserve(32);

            for (int y = 0; y < 8; ++y) {//기물 생성
                pieces2.push_back(std::make_unique<pieces>(1, backrank[y], vector{0, y}));
                pieces2.push_back(std::make_unique<pieces>(1, 1, vector{1, y}));
                pieces2.push_back(std::make_unique<pieces>(-1, 1, vector{6, y}));
                pieces2.push_back(std::make_unique<pieces>(-1, backrank[y], vector{7, y}));
            }

            rendergame(true);
            std::thread timer([this]() {//타이머 시작
                while (!endgame) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    std::lock_guard<std::mutex> lock(display_mutex);
                    render_status();
                }
            });

            while(!endgame){
                updategame();
            }

            timer.join();
        }
        void updategame(){//업데이트
            int from_x;
            int from_y;
            int to_x;
            int to_y;

            std::cout << "선택할 기물의 좌표를 입력하세요(fx fy): " << std::flush;
            if (!(std::cin >> from_x >> from_y)) {
                endgame = true;
                return;
            }

            if (!is_on_board(from_x, from_y)) {
                std::cout << "잘못된 좌표입니다.\n";
                return;
            }

            pieces* selected_piece = find_piece_at(from_x, from_y);

            if (selected_piece == nullptr) {
                std::cout << "해당 위치에 기물이 없습니다.\n";
                return;
            }

            if (!director.can_select(*selected_piece)) {
                std::cout << "현재 턴의 색상과 다른 기물입니다.\n";
                return;
            }

            vector selected_position{from_x, from_y};
            std::vector<vector> legal_moves;
            std::vector<vector> blocked_moves;
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    vector destination{x, y};
                    if (selected_piece->can_move_to(destination, &board)) {
                        legal_moves.push_back(destination);
                    } else if (selected_piece->is_publicly_valid_destination(destination, &board)) {
                        blocked_moves.push_back(destination);
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock(display_mutex);
                rendergame_unlocked(true, &selected_position, &legal_moves, &blocked_moves);
            }

            std::cout << "목적지 좌표를 입력하세요(tx ty): " << std::flush;
            if (!(std::cin >> to_x >> to_y)) {
                endgame = true;
                return;
            }

            if (!is_on_board(to_x, to_y)) {
                std::cout << "잘못된 좌표입니다.\n";
                return;
            }

            if (!board.is_path_clear(selected_position, vector{to_x, to_y}, selected_piece->gettype())) {
                std::cout << "해당 방향에 장애물이 있어 이동할 수 없습니다.\n";
                return;
            }

            // 목적지의 상대 기물을 먼저 찾고 생존 상태를 죽음으로 바꾼 뒤,
            // 실제 기물의 좌표는 공격자 위치에서 목적지로 옮겨야 한다.
            pieces* captured_piece = find_piece_at(to_x, to_y);
            const bool will_capture_enemy =
                captured_piece != nullptr &&
                captured_piece != selected_piece &&
                captured_piece->getcolor() != selected_piece->getcolor();

            std::lock_guard<std::mutex> lock(display_mutex);
            if (director.move_piece(*selected_piece, to_x, to_y, &board)) {
                // 이동이 성공한 경우에만 기물 객체의 위치를 목적지로 갱신한다.
                selected_piece->move(to_x, to_y, &board);

                // 보드 배열은 칸 번호만 보관하는 구조라서, 잡힌 상대 기물이
                // 있으면 그 칸을 비우고, 이동하는 기물의 숫자는 새 칸으로 이동시킨다.
                if (will_capture_enemy) {
                    captured_piece->set_alive(false);
                    board.remove_piece_at(vector{to_x, to_y});
                }

                board.move_piece(selected_position, vector{to_x, to_y});
                rendergame_unlocked(true);
            } else {
                std::cout << "이동할 수 없습니다.\n";
            }
        };
        void rendergame(bool reset_cursor = false){
            std::lock_guard<std::mutex> lock(display_mutex);
            rendergame_unlocked(reset_cursor);
        }

        void rendergame_unlocked(bool reset_cursor,
                                  const vector* selected = nullptr,
                                  const std::vector<vector>* legal_moves = nullptr,
                                  const std::vector<vector>* blocked_moves = nullptr){
            if (reset_cursor) {
                std::cout << "\033[2J\033[H";
            }
            render_status();
            std::cout << "\033[2;1H";
            board.printbord(selected, legal_moves, blocked_moves);
        }

    private:
        void render_status(){
            // for(int i=0; i<pieces2.size(); i++){
            //     std::cout << "type : " << pieces2[i].
            // }
            
            std::cout << "\033[s\033[1;1H";
            std::cout << "턴: " << director.get_turn_count()
                      << ", 현재 색상: "
                      << (director.get_current_color() == 1 ? "흰색" : "검은색")
                      << ", 경과 시간: "
                      << director.get_turn_time().count() / 1000.0 << "초   ";
            std::cout << "\033[u" << std::flush;
        }

        pieces* find_piece_at(int x, int y) {
            auto it = std::find_if(pieces2.begin(), pieces2.end(),
                [x, y](const std::unique_ptr<pieces>& piece) {
                    if (!piece->is_alive()) {
                        return false;
                    }

                    vector position = piece->position();
                    return position.x == x && position.y == y;
                });

            if (it == pieces2.end()) {
                return nullptr;
            }

            return it->get();
        }

        static bool is_on_board(int x, int y){
            return x >= 0 && x < 8 && y >= 0 && y < 8;
        }

        Board board;
        std::vector<std::unique_ptr<pieces>> pieces2;
        Director director;
        std::mutex display_mutex;

};