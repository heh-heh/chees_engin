#include <iostream>
#include <memory>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>
#include "bord.cpp"
#include "../manager/Director.cpp"
#include "../object/pieces.cpp"

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
        void updategame();
        void rendergame(bool reset_cursor = false);
        void rendergame_unlocked(bool reset_cursor,
                                  const vector* selected = nullptr,
                                  const std::vector<vector>* legal_moves = nullptr,
                                  const std::vector<vector>* blocked_moves = nullptr);

    private:
        void render_status();

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

#include "update.cpp"
#include "render.cpp"