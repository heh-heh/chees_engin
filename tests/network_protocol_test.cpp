#include "../src/network/chess_protocol.hpp"

#include <cassert>
#include <string>

int main() {
    const std::string board = "4k3/8/8/8/8/8/8/4K3";
    const auto message = chess::build_board_message(board, 1);

    assert(message.starts_with("BOARD|"));
    assert(message.find(board) != std::string::npos);

    const auto parsed = chess::parse_board_message(message);
    assert(parsed.board == board);
    assert(parsed.turn == 1);

    return 0;
}
