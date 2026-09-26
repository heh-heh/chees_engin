#include "../src/network/chess_protocol.hpp"

#include <cassert>
#include <string>

int main() {
    const std::string board = "4k3/8/8/8/8/8/8/4K3";
    const auto message = chess::build_board_message(board, 1);

    assert(message.rfind("BOARD|", 0) == 0);
    assert(message.find(board) != std::string::npos);

    const auto parsed = chess::parse_board_message(message);
    assert(parsed.board == board);
    assert(parsed.turn == 1);

    const auto chat = chess::build_chat_message("Good luck!");
    std::string chat_text;
    assert(chess::parse_chat_message(chat, chat_text));
    assert(chat_text == "Good luck!");

    bool ready = false;
    assert(chess::parse_ready_message(chess::build_ready_message(true), ready));
    assert(ready);
    assert(chess::is_match_start_message(chess::build_match_start_message()));

    const auto state = chess::build_match_state_message(12, -1, 45678, 135);
    chess::MatchStateMessage parsed_state;
    assert(chess::parse_match_state_message(state, parsed_state));
    assert(parsed_state.turn == 12);
    assert(parsed_state.current_color == -1);
    assert(parsed_state.turn_elapsed_ms == 45678);
    assert(parsed_state.score == 135);
    assert(!chess::parse_match_state_message("STATE|0|1|0|0", parsed_state));
    assert(!chess::parse_match_state_message("STATE|2|0|10|0", parsed_state));
    assert(!chess::parse_chat_message("CHAT|", chat_text));

    return 0;
}
