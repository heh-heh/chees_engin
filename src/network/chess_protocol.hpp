#pragma once

#include <string>
#include <utility>

namespace chess {

struct BoardMessage {
    std::string board;
    int turn = 0;
};

struct MatchStateMessage {
    int turn = 0;
    int current_color = 0;
    long long turn_elapsed_ms = 0;
    int score = 0;
    bool valid = false;
};

inline std::string build_board_message(const std::string& board_fen, int turn_value) {
    return "BOARD|" + board_fen + "|" + std::to_string(turn_value);
}

inline BoardMessage parse_board_message(const std::string& message) {
    BoardMessage parsed;
    const std::string prefix = "BOARD|";
    if (message.rfind(prefix, 0) != 0) {
        return parsed;
    }

    const std::string payload = message.substr(prefix.size());
    const std::size_t split = payload.find('|');
    if (split == std::string::npos) {
        return parsed;
    }

    parsed.board = payload.substr(0, split);
    const std::string turn_text = payload.substr(split + 1);
    parsed.turn = std::stoi(turn_text);
    return parsed;
}

inline std::string build_chat_message(const std::string& text) {
    return "CHAT|" + text;
}

inline bool parse_chat_message(const std::string& message, std::string& text) {
    const std::string prefix = "CHAT|";
    if (message.rfind(prefix, 0) != 0 || message.size() == prefix.size()) {
        return false;
    }

    text = message.substr(prefix.size());
    return true;
}

inline std::string build_ready_message(bool ready) {
    return ready ? "READY|1" : "READY|0";
}

inline bool parse_ready_message(const std::string& message, bool& ready) {
    if (message == "READY|1") {
        ready = true;
        return true;
    }
    if (message == "READY|0") {
        ready = false;
        return true;
    }
    return false;
}

inline std::string build_match_start_message() {
    return "START";
}

inline bool is_match_start_message(const std::string& message) {
    return message == "START";
}

inline std::string build_match_state_message(int turn, int current_color, long long turn_elapsed_ms, int score) {
    return "STATE|" + std::to_string(turn) + "|" + std::to_string(current_color) + "|" +
           std::to_string(turn_elapsed_ms) + "|" + std::to_string(score);
}

inline bool parse_match_state_message(const std::string& message, MatchStateMessage& state) {
    const std::string prefix = "STATE|";
    if (message.rfind(prefix, 0) != 0) {
        return false;
    }

    const std::string payload = message.substr(prefix.size());
    const std::size_t first = payload.find('|');
    const std::size_t second = first == std::string::npos ? std::string::npos : payload.find('|', first + 1);
    const std::size_t third = second == std::string::npos ? std::string::npos : payload.find('|', second + 1);
    if (first == std::string::npos || second == std::string::npos || third == std::string::npos ||
        payload.find('|', third + 1) != std::string::npos) {
        return false;
    }

    try {
        state.turn = std::stoi(payload.substr(0, first));
        state.current_color = std::stoi(payload.substr(first + 1, second - first - 1));
        state.turn_elapsed_ms = std::stoll(payload.substr(second + 1, third - second - 1));
        state.score = std::stoi(payload.substr(third + 1));
    } catch (const std::exception&) {
        return false;
    }

    state.valid = state.turn > 0 && (state.current_color == 1 || state.current_color == -1) &&
                  state.turn_elapsed_ms >= 0;
    return state.valid;
}

}  // namespace chess
