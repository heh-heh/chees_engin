#include <iostream>
#include <memory>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
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

            for (int y = 0; y < 8; ++y) {
                pieces2.push_back(std::make_unique<pieces>(1, backrank[y], vector{0, y}));
                pieces2.push_back(std::make_unique<pieces>(1, 1, vector{1, y}));
                pieces2.push_back(std::make_unique<pieces>(-1, 1, vector{6, y}));
                pieces2.push_back(std::make_unique<pieces>(-1, backrank[y], vector{7, y}));
            }

            rendergame(true);
            std::thread timer([this]() {
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
        void updategame(){
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

            pieces* selected_piece = nullptr;
            for (auto& piece : pieces2) {
                vector position = piece->position();

                if (position.x == from_x && position.y == from_y) {
                    selected_piece = piece.get();
                    break;
                }
            }

            if (selected_piece == nullptr) {
                std::cout << "해당 위치에 기물이 없습니다.\n";
                return;
            }

            if (!director.can_select(*selected_piece)) {
                std::cout << "현재 턴의 색상과 다른 기물입니다.\n";
                return;
            }

            vector selected_position{from_x, from_y};
            {
                std::lock_guard<std::mutex> lock(display_mutex);
                rendergame_unlocked(true, &selected_position);
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

            std::lock_guard<std::mutex> lock(display_mutex);
            if (director.move_piece(*selected_piece, to_x, to_y)) {
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

        void rendergame_unlocked(bool reset_cursor, const vector* selected = nullptr){
            if (reset_cursor) {
                std::cout << "\033[2J\033[H";
            }
            render_status();
            std::cout << "\033[2;1H";
            board.printbord(selected);
        }

    private:
        void render_status(){
            std::cout << "\033[s\033[1;1H";
            std::cout << "턴: " << director.get_turn_count()
                      << ", 현재 색상: "
                      << (director.get_current_color() == 1 ? "흰색" : "검은색")
                      << ", 경과 시간: "
                      << director.get_turn_time().count() / 1000.0 << "초   ";
            std::cout << "\033[u" << std::flush;
        }

        static bool is_on_board(int x, int y){
            return x >= 0 && x < 8 && y >= 0 && y < 8;
        }

        Board board;
        std::vector<std::unique_ptr<pieces>> pieces2;
        Director director;
        std::mutex display_mutex;

};