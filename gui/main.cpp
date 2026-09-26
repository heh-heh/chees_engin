#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

#include "../src/game/bord.cpp"
#include "../src/manager/Director.cpp"
#include "../src/network/chess_protocol.hpp"
#include "../src/object/pieces.cpp"

namespace {

using BoardState = std::array<std::array<int, 8>, 8>;

enum class Screen {
    Start,
    Lobby,
    ModeSelect,
    WaitingRoom,
    Game
};

enum class MultiplayerMode {
    None,
    Host,
    Client
};

bool initialize_socket_runtime() {
#ifdef _WIN32
    static bool initialized = false;
    if (initialized) {
        return true;
    }
    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return false;
    }
    initialized = true;
    return true;
#else
    return true;
#endif
}

struct TcpConnection {
    SOCKET socket_fd = INVALID_SOCKET;
    bool is_server = false;
    std::string receive_buffer;

    ~TcpConnection() {
        close();
    }

    void close() {
        if (socket_fd != INVALID_SOCKET) {
#ifdef _WIN32
            closesocket(socket_fd);
#else
            ::close(socket_fd);
#endif
            socket_fd = INVALID_SOCKET;
        }
    }

    bool start_server(int port, std::string& error) {
        if (!initialize_socket_runtime()) {
            error = "Socket initialization failed.";
            return false;
        }
        if (port < 1 || port > 65535) {
            error = "Port must be between 1 and 65535.";
            return false;
        }

        socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd == INVALID_SOCKET) {
            error = "Failed to create listening socket.";
            return false;
        }

        int reuse = 1;
        setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(static_cast<unsigned short>(port));

        if (bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
            error = "Port is already in use.";
            close();
            return false;
        }

        if (listen(socket_fd, 1) == SOCKET_ERROR) {
            error = "Listen failed.";
            close();
            return false;
        }

        is_server = true;
        receive_buffer.clear();
        return true;
    }

    bool accept_client(std::string& error) {
        if (!is_server || socket_fd == INVALID_SOCKET) {
            error = "Server socket is not active.";
            return false;
        }

        sockaddr_in client_address{};
    #ifdef _WIN32
        int client_length = sizeof(client_address);
    #else
        socklen_t client_length = sizeof(client_address);
    #endif
        const SOCKET peer = accept(socket_fd, reinterpret_cast<sockaddr*>(&client_address), &client_length);
        if (peer == INVALID_SOCKET) {
            error = "Client connection failed.";
            return false;
        }

        if (socket_fd != INVALID_SOCKET) {
#ifdef _WIN32
            closesocket(socket_fd);
#else
            ::close(socket_fd);
#endif
            socket_fd = INVALID_SOCKET;
        }

        socket_fd = peer;
        is_server = false;
        receive_buffer.clear();
        return true;
    }

    bool connect_to_server(const std::string& host, int port, std::string& error) {
        if (!initialize_socket_runtime()) {
            error = "Socket initialization failed.";
            return false;
        }
        if (host.empty()) {
            error = "Enter the host LAN address.";
            return false;
        }
        if (port < 1 || port > 65535) {
            error = "Port must be between 1 and 65535.";
            return false;
        }

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* results = nullptr;
        if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &results) != 0 || results == nullptr) {
            error = "Host address could not be resolved.";
            return false;
        }

        socket_fd = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
        if (socket_fd == INVALID_SOCKET) {
            freeaddrinfo(results);
            error = "Failed to create client socket.";
            return false;
        }

        const int connect_result = connect(socket_fd, results->ai_addr, static_cast<int>(results->ai_addrlen));
        freeaddrinfo(results);
        if (connect_result == SOCKET_ERROR) {
            error = "Unable to connect to host.";
            close();
            return false;
        }

        receive_buffer.clear();
        return true;
    }

    bool send_all(const std::string& message) {
        if (socket_fd == INVALID_SOCKET) {
            return false;
        }

        const char* data = message.c_str();
        size_t total_sent = 0;
        while (total_sent < message.size()) {
            const int sent = send(socket_fd, data + total_sent, static_cast<int>(message.size() - total_sent), 0);
            if (sent <= 0) {
                return false;
            }
            total_sent += static_cast<size_t>(sent);
        }
        return true;
    }

    bool try_receive_message(std::string& message, bool& disconnected) {
        disconnected = false;
        if (socket_fd == INVALID_SOCKET) {
            disconnected = true;
            return false;
        }

        const auto buffered_line = receive_buffer.find('\n');
        if (buffered_line != std::string::npos) {
            message = receive_buffer.substr(0, buffered_line);
            receive_buffer.erase(0, buffered_line + 1);
            return true;
        }

        fd_set readable_sockets;
        FD_ZERO(&readable_sockets);
        FD_SET(socket_fd, &readable_sockets);
        timeval timeout{};
        const int select_result = select(static_cast<int>(socket_fd) + 1, &readable_sockets, nullptr, nullptr, &timeout);
        if (select_result <= 0 || !FD_ISSET(socket_fd, &readable_sockets)) {
            return false;
        }

        char buffer[512]{};
        const int received = recv(socket_fd, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            disconnected = true;
            close();
            return false;
        }

        receive_buffer.append(buffer, static_cast<size_t>(received));
        const auto line_end = receive_buffer.find('\n');
        if (line_end == std::string::npos) {
            return false;
        }

        message = receive_buffer.substr(0, line_end);
        receive_buffer.erase(0, line_end + 1);
        return true;
    }
};

struct GameSession {
    Board engine_board;
    std::vector<std::unique_ptr<pieces>> engine_pieces;
    Director engine_director;
    BoardState board{};
    int turn = 1;
    int current_color = 1;
    int selected_row = -1;
    int selected_col = -1;
    std::vector<std::pair<int, int>> legal_moves;
    std::string status_message = "Select a piece to move.";
    bool is_multiplayer = false;
    bool is_game_over = false;
    int winner_color = 0;
    MultiplayerMode multiplayer_mode = MultiplayerMode::None;
    bool network_ready = false;
    std::string network_host = "127.0.0.1";
    std::array<char, 256> network_host_input{"127.0.0.1"};
    std::string room_code = "ROOM-9001";
    std::array<char, 64> room_code_input{"ROOM-9001"};
    int network_port = 9001;
    int local_player_color = 1;
    std::string network_error = "";
    bool local_ready = false;
    bool opponent_ready = false;
    bool remote_start_requested = false;
    bool has_remote_match_state = false;
    std::chrono::milliseconds synced_turn_elapsed{0};
    int synced_score = 0;
    std::chrono::steady_clock::time_point last_remote_state_at{};
    std::chrono::steady_clock::time_point last_state_broadcast{};
    std::array<char, 256> chat_input{};
    std::vector<std::string> chat_history;
    TcpConnection network_connection;
    std::thread network_thread;
    std::mutex network_mutex;
    bool waiting_room_active = false;
};

void append_chat_message(GameSession& session, const std::string& sender, const std::string& text) {
    session.chat_history.push_back(sender + ": " + text);
    if (session.chat_history.size() > 40) {
        session.chat_history.erase(session.chat_history.begin());
    }
}

bool send_network_message(GameSession& session, const std::string& message) {
    if (!session.network_ready || session.network_connection.socket_fd == INVALID_SOCKET ||
        !session.network_connection.send_all(message + "\n")) {
        session.network_ready = false;
        session.status_message = "Network message could not be sent.";
        return false;
    }
    return true;
}

void send_chat_message(GameSession& session) {
    const std::string text = session.chat_input.data();
    if (text.empty()) {
        return;
    }

    if (send_network_message(session, chess::build_chat_message(text))) {
        append_chat_message(session, "You", text);
        session.chat_input[0] = '\0';
    }
}

void broadcast_match_state(GameSession& session, bool force = false) {
    if (!session.is_multiplayer || session.multiplayer_mode != MultiplayerMode::Host || !session.network_ready) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (!force && session.last_state_broadcast.time_since_epoch().count() != 0 &&
        now - session.last_state_broadcast < std::chrono::seconds(1)) {
        return;
    }

    const std::string state_message = chess::build_match_state_message(
        session.turn,
        session.current_color,
        session.engine_director.get_turn_time().count(),
        session.engine_director.evaluate_board(session.engine_board));
    if (send_network_message(session, state_message)) {
        session.last_state_broadcast = now;
    }
}

void stop_network_session(GameSession& session) {
    session.network_connection.close();
    if (session.network_thread.joinable()) {
        session.network_thread.join();
    }
    session.network_ready = false;
    session.waiting_room_active = false;
    session.local_ready = false;
    session.opponent_ready = false;
    session.remote_start_requested = false;
    session.has_remote_match_state = false;
    session.chat_history.clear();
    session.chat_input[0] = '\0';
}

std::string local_ipv4_address() {
    char hostname[256]{};
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return "Unavailable";
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    addrinfo* results = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &results) != 0 || results == nullptr) {
        return "Unavailable";
    }

    std::string address = "Unavailable";
    for (addrinfo* entry = results; entry != nullptr; entry = entry->ai_next) {
        const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(entry->ai_addr);
        if (ntohl(ipv4->sin_addr.s_addr) == INADDR_LOOPBACK) {
            continue;
        }

        char text[INET_ADDRSTRLEN]{};
        if (inet_ntop(AF_INET, &ipv4->sin_addr, text, sizeof(text)) != nullptr) {
            address = text;
            break;
        }
    }

    freeaddrinfo(results);
    return address;
}

BoardState board_state_from_pieces(const std::vector<std::unique_ptr<pieces>>& pieces) {
    BoardState state{};
    for (const auto& piece : pieces) {
        if (!piece->is_alive()) {
            continue;
        }

        const vector position = piece->position();
        state[position.y][position.x] = piece->getcolor() * piece->gettype();
    }
    return state;
}

void sync_engine_board(Board& board_state, const std::vector<std::unique_ptr<pieces>>& pieces) {
    board_state.clear_board();
    for (const auto& piece : pieces) {
        if (!piece->is_alive()) {
            continue;
        }

        const vector position = piece->position();
        board_state.set_piece_at(position, piece->getcolor() * piece->gettype());
    }
}

vector engine_pos_from_ui(int row, int col) {
    return {col, row};
}

pieces* find_piece_at_position(std::vector<std::unique_ptr<pieces>>& pieces, int row, int col) {
    const vector engine_pos = engine_pos_from_ui(row, col);
    for (auto& piece : pieces) {
        if (!piece->is_alive()) {
            continue;
        }

        const vector position = piece->position();
        if (position.x == engine_pos.x && position.y == engine_pos.y) {
            return piece.get();
        }
    }
    return nullptr;
}

bool would_leave_king_in_check(const Board& board_state, const pieces& moving_piece, const vector& destination) {
    Board candidate = board_state;
    const vector from = moving_piece.position();
    const int moving_color = moving_piece.getcolor();

    const bool en_passant_capture = moving_piece.gettype() == 1 &&
        std::abs(destination.x - from.x) == 1 &&
        !candidate.has_piece(destination) &&
        candidate.is_en_passant_target(destination, moving_color);

    if (en_passant_capture) {
        const vector captured_position{destination.x, destination.y - (moving_color == 1 ? -1 : 1)};
        candidate.remove_piece_at(captured_position);
    }

    if (moving_piece.gettype() == 6 && std::abs(destination.x - from.x) == 2) {
        const int rook_from_x = destination.x > from.x ? 7 : 0;
        const int rook_to_x = destination.x > from.x ? 5 : 3;
        candidate.move_piece({rook_from_x, from.y}, {rook_to_x, from.y});
    }

    candidate.move_piece(from, destination);
    return candidate.is_king_in_check(moving_color);
}

std::vector<std::pair<int, int>> collect_legal_moves_for_piece(
    const std::vector<std::unique_ptr<pieces>>& piece_list,
    pieces* selected_piece,
    const Board& board_state) {
    (void)piece_list;
    std::vector<std::pair<int, int>> moves;
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const vector destination = engine_pos_from_ui(row, col);
            if (!selected_piece->can_move_to(destination, &board_state)) {
                continue;
            }
            if (would_leave_king_in_check(board_state, *selected_piece, destination)) {
                continue;
            }
            moves.emplace_back(row, col);
        }
    }
    return moves;
}

void initialize_engine_pieces(std::vector<std::unique_ptr<pieces>>& piece_list) {
    piece_list.clear();
    const int backrank[8] = {4, 2, 3, 5, 6, 3, 2, 4};

    for (int x = 0; x < 8; ++x) {
        piece_list.push_back(std::make_unique<pieces>(1, backrank[x], vector{x, 7}));
        piece_list.push_back(std::make_unique<pieces>(1, 1, vector{x, 6}));
        piece_list.push_back(std::make_unique<pieces>(-1, 1, vector{x, 1}));
        piece_list.push_back(std::make_unique<pieces>(-1, backrank[x], vector{x, 0}));
    }
}

void reset_game_session(GameSession& session) {
    initialize_engine_pieces(session.engine_pieces);
    session.engine_board = Board{};
    sync_engine_board(session.engine_board, session.engine_pieces);
    session.engine_director = Director{};
    session.board = board_state_from_pieces(session.engine_pieces);
    session.turn = 1;
    session.current_color = session.engine_director.get_current_color();
    session.selected_row = -1;
    session.selected_col = -1;
    session.legal_moves.clear();
    session.is_game_over = false;
    session.winner_color = 0;
    session.has_remote_match_state = false;
    session.synced_turn_elapsed = std::chrono::milliseconds{0};
    session.status_message = session.is_multiplayer ? "Multiplayer match ready." : "Single-player match ready.";
}

struct RemoteMove {
    int from_row = -1;
    int from_col = -1;
    int to_row = -1;
    int to_col = -1;
    int color = 0;
    bool valid = false;
};

std::string build_remote_move_message(int from_row, int from_col, int to_row, int to_col, int color) {
    return "MOVE|" + std::to_string(from_row) + "|" + std::to_string(from_col) + "|" +
           std::to_string(to_row) + "|" + std::to_string(to_col) + "|" + std::to_string(color);
}

bool parse_remote_move_message(const std::string& message, RemoteMove& move) {
    if (message.rfind("MOVE|", 0) != 0) {
        return false;
    }

    const std::string payload = message.substr(5);
    std::vector<std::string> parts;
    std::size_t start = 0;
    std::size_t end = payload.find('|');
    while (end != std::string::npos) {
        parts.push_back(payload.substr(start, end - start));
        start = end + 1;
        end = payload.find('|', start);
    }
    parts.push_back(payload.substr(start));

    if (parts.size() != 5) {
        return false;
    }

    try {
        move.from_row = std::stoi(parts[0]);
        move.from_col = std::stoi(parts[1]);
        move.to_row = std::stoi(parts[2]);
        move.to_col = std::stoi(parts[3]);
        move.color = std::stoi(parts[4]);
    } catch (const std::exception&) {
        return false;
    }
    move.valid = true;
    return true;
}

void apply_remote_move(GameSession& session, const RemoteMove& move) {
    if (!move.valid) {
        return;
    }

    pieces* remote_piece = find_piece_at_position(session.engine_pieces, move.from_row, move.from_col);
    if (remote_piece == nullptr || remote_piece->getcolor() != move.color) {
        return;
    }

    const vector from = engine_pos_from_ui(move.from_row, move.from_col);
    const vector to = engine_pos_from_ui(move.to_row, move.to_col);
    pieces* captured_piece = find_piece_at_position(session.engine_pieces, move.to_row, move.to_col);
    if (session.engine_director.move_piece(*remote_piece, to.x, to.y, &session.engine_board)) {
        if (captured_piece != nullptr && captured_piece != remote_piece) {
            captured_piece->set_alive(false);
            session.engine_board.remove_piece_at(to);
        }
        session.engine_board.move_piece(from, to);
        sync_engine_board(session.engine_board, session.engine_pieces);
        session.selected_row = -1;
        session.selected_col = -1;
        session.legal_moves.clear();
        session.board = board_state_from_pieces(session.engine_pieces);
        session.current_color = session.engine_director.get_current_color();
        session.turn = static_cast<int>(session.engine_director.get_turn_count()) + 1;
        session.status_message = "Opponent moved. Your turn.";
    }
}

void process_incoming_network_messages(GameSession& session) {
    if (!session.is_multiplayer || !session.network_ready) {
        return;
    }

    std::string message;
    bool disconnected = false;
    while (session.network_connection.try_receive_message(message, disconnected)) {
        RemoteMove move;
        if (parse_remote_move_message(message, move)) {
            const int expected_color = session.multiplayer_mode == MultiplayerMode::Host ? -1 : 1;
            if (move.color == expected_color) {
                apply_remote_move(session, move);
                if (session.multiplayer_mode == MultiplayerMode::Host) {
                    broadcast_match_state(session, true);
                }
            }
            continue;
        }

        bool ready = false;
        if (chess::parse_ready_message(message, ready)) {
            session.opponent_ready = ready;
            session.status_message = ready ? "Opponent is ready." : "Opponent is not ready.";
            continue;
        }

        if (chess::is_match_start_message(message)) {
            if (session.multiplayer_mode == MultiplayerMode::Client) {
                session.remote_start_requested = true;
                session.status_message = "Host started the match.";
            }
            continue;
        }

        std::string chat_text;
        if (chess::parse_chat_message(message, chat_text)) {
            append_chat_message(session, "Opponent", chat_text);
            continue;
        }

        chess::MatchStateMessage state;
        if (session.multiplayer_mode == MultiplayerMode::Client && chess::parse_match_state_message(message, state)) {
            session.turn = state.turn;
            session.current_color = state.current_color;
            session.synced_turn_elapsed = std::chrono::milliseconds{state.turn_elapsed_ms};
            session.synced_score = state.score;
            session.last_remote_state_at = std::chrono::steady_clock::now();
            session.has_remote_match_state = true;
        }
    }

    if (disconnected) {
        session.network_ready = false;
        session.status_message = "Opponent disconnected.";
    }
}

void render_chat_panel(GameSession& session, const char* panel_id, float height) {
    ImGui::TextColored(ImVec4(0.65f, 0.78f, 1.0f, 1.0f), "MATCH CHAT");
    ImGui::BeginChild(panel_id, ImVec2(0.0f, height), true);
    for (const std::string& chat_line : session.chat_history) {
        ImGui::TextWrapped("%s", chat_line.c_str());
    }
    if (!session.chat_history.empty()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    const bool submitted = ImGui::InputText("##chat_input", session.chat_input.data(), session.chat_input.size(),
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((submitted || ImGui::Button("Send")) && session.network_ready) {
        send_chat_message(session);
    }
}

std::string piece_symbol(int piece_value) {
    if (piece_value == 0) {
        return "";
    }

    const bool is_white = piece_value > 0;
    const int type = std::abs(piece_value);

    switch (type) {
        case 1: return is_white ? "P" : "p";
        case 2: return is_white ? "N" : "n";
        case 3: return is_white ? "B" : "b";
        case 4: return is_white ? "R" : "r";
        case 5: return is_white ? "Q" : "q";
        case 6: return is_white ? "K" : "k";
        default: return "";
    }
}

std::string color_name(int color) {
    return color == 1 ? "White" : "Black";
}

std::string format_turn_timer(std::chrono::milliseconds elapsed) {
    const auto total_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed);
    const int seconds = static_cast<int>(total_seconds.count() % 60);
    const int minutes = static_cast<int>(total_seconds.count() / 60);
    return std::to_string(minutes) + ":" + (seconds < 10 ? "0" : "") + std::to_string(seconds);
}

std::string format_score_value(int score) {
    const std::string sign = score >= 0 ? "+" : "-";
    const int magnitude = std::abs(score);
    const int whole = magnitude / 100;
    const int fraction = magnitude % 100;
    return sign + std::to_string(whole) + "." + (fraction < 10 ? "0" : "") + std::to_string(fraction);
}

std::pair<int, int> find_king_position(const BoardState& board, int color) {
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const int value = board[row][col];
            if (value != 0 && std::abs(value) == 6 && (value > 0) == (color > 0)) {
                return {row, col};
            }
        }
    }
    return {-1, -1};
}

void render_start_screen(Screen& current_screen, bool& should_close) {
    ImGui::SetNextWindowPos(ImVec2(200.0f, 140.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(880.0f, 440.0f), ImGuiCond_Always);
    ImGui::Begin("Start Screen", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::Spacing();
    ImGui::Text("Chess Engine");
    ImGui::TextDisabled("Lobby-based GUI for multiplayer-ready chess");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushTextWrapPos(760.0f);
    ImGui::TextUnformatted("This is the start screen. Move to the lobby to choose single-player or multiplayer mode.");
    ImGui::PopTextWrapPos();

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::Button("Start", ImVec2(200.0f, 50.0f))) {
        current_screen = Screen::Lobby;
    }

    ImGui::SameLine();
    if (ImGui::Button("Exit", ImVec2(200.0f, 50.0f))) {
        should_close = true;
    }

    ImGui::End();
}

void render_lobby_screen(Screen& current_screen) {
    ImGui::SetNextWindowPos(ImVec2(200.0f, 140.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(880.0f, 440.0f), ImGuiCond_Always);
    ImGui::Begin("Lobby", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Lobby");
    ImGui::TextDisabled("Choose a game mode and prepare a match.");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Play Mode");
    ImGui::Spacing();

    if (ImGui::Button("Single Player", ImVec2(230.0f, 90.0f))) {
        current_screen = Screen::ModeSelect;
    }

    ImGui::SameLine();
    if (ImGui::Button("Multiplayer", ImVec2(230.0f, 90.0f))) {
        current_screen = Screen::ModeSelect;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Match Lobby");
    ImGui::BeginChild("LobbyStatus", ImVec2(0.0f, 140.0f), true);
    ImGui::Text("- Quick start: local single-player");
    ImGui::Text("- Multiplayer: host and join a match");
    ImGui::Text("- Status: waiting in lobby");
    ImGui::EndChild();

    ImGui::Spacing();
    if (ImGui::Button("Back", ImVec2(160.0f, 40.0f))) {
        current_screen = Screen::Start;
    }

    ImGui::End();
}

void render_mode_select_screen(Screen& current_screen, GameSession& session) {
    ImGui::SetNextWindowPos(ImVec2(200.0f, 140.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(880.0f, 440.0f), ImGuiCond_Always);
    ImGui::Begin("Mode Select", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Game Mode Selection");
    ImGui::TextDisabled("Choose how the match will begin.");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Start Single Player", ImVec2(270.0f, 90.0f))) {
        session.is_multiplayer = false;
        session.multiplayer_mode = MultiplayerMode::None;
        session.local_player_color = 1;
        session.network_ready = false;
        session.network_error.clear();
        stop_network_session(session);
        reset_game_session(session);
        current_screen = Screen::Game;
    }

    ImGui::SameLine();
    if (ImGui::Button("Host Match", ImVec2(200.0f, 90.0f))) {
        session.is_multiplayer = true;
        session.multiplayer_mode = MultiplayerMode::Host;
        session.local_player_color = 1;
        session.network_error.clear();
        session.network_ready = false;
        session.waiting_room_active = true;
        session.room_code = "ROOM-" + std::to_string(session.network_port);
        if (session.network_thread.joinable()) {
            session.network_thread.join();
        }

        std::string error;
        if (session.network_connection.start_server(session.network_port, error)) {
            session.status_message = "Room created. Waiting for opponent...";
            session.network_ready = false;
            session.network_thread = std::thread([&session]() {
                std::string thread_error;
                if (session.network_connection.accept_client(thread_error)) {
                    std::lock_guard<std::mutex> lock(session.network_mutex);
                    session.network_ready = true;
                    session.status_message = "Opponent joined. Match is ready.";
                    session.network_error.clear();
                } else {
                    std::lock_guard<std::mutex> lock(session.network_mutex);
                    session.network_error = thread_error;
                    session.status_message = "No opponent connected yet.";
                }
            });
            current_screen = Screen::WaitingRoom;
        } else {
            session.network_error = error;
            session.status_message = "Host failed: " + error;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Join Match", ImVec2(200.0f, 90.0f))) {
        session.is_multiplayer = true;
        session.multiplayer_mode = MultiplayerMode::Client;
        session.local_player_color = -1;
        session.network_error.clear();
        session.network_ready = false;
        session.waiting_room_active = true;
        if (session.network_thread.joinable()) {
            session.network_thread.join();
        }
        session.network_connection.close();
        session.network_host = session.network_host_input.data();
        session.room_code = session.room_code_input.data();
        session.room_code = session.room_code.empty() ? "ROOM-" + std::to_string(session.network_port) : session.room_code;

        session.network_thread = std::thread([&session]() {
            std::string thread_error;
            if (session.network_connection.connect_to_server(session.network_host, session.network_port, thread_error)) {
                std::lock_guard<std::mutex> lock(session.network_mutex);
                session.network_ready = true;
                session.network_error.clear();
                session.status_message = "Connected to host. Match is ready.";
            } else {
                std::lock_guard<std::mutex> lock(session.network_mutex);
                session.network_error = thread_error;
                session.status_message = "Connection failed: " + thread_error;
            }
        });
        current_screen = Screen::WaitingRoom;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Network Options");
    ImGui::InputText("Host", session.network_host_input.data(), session.network_host_input.size());
    ImGui::InputText("Room Code", session.room_code_input.data(), session.room_code_input.size());
    ImGui::InputInt("Port", &session.network_port);
    if (!session.network_error.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", session.network_error.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.7f, 1.0f), "Mode: %s", session.multiplayer_mode == MultiplayerMode::Host ? "Host" : session.multiplayer_mode == MultiplayerMode::Client ? "Client" : "None");
    }

    ImGui::Spacing();
    ImGui::BeginChild("ModeInfo", ImVec2(0.0f, 120.0f), true);
    ImGui::Text("Current Setup");
    ImGui::Text("- Single Player: local board and turn logic");
    ImGui::Text("- Multiplayer: host and client with TCP socket sync");
    ImGui::Text("- Waiting room appears until the other player connects");
    ImGui::EndChild();

    ImGui::Spacing();
    if (ImGui::Button("Back to Lobby", ImVec2(200.0f, 40.0f))) {
        current_screen = Screen::Lobby;
    }

    ImGui::End();
}

void render_waiting_room_screen(Screen& current_screen, GameSession& session) {
    process_incoming_network_messages(session);

    if (session.multiplayer_mode == MultiplayerMode::Client && session.remote_start_requested) {
        reset_game_session(session);
        session.remote_start_requested = false;
        current_screen = Screen::Game;
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(200.0f, 120.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(900.0f, 560.0f), ImGuiCond_Always);
    ImGui::Begin("Waiting Room", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    if (session.multiplayer_mode == MultiplayerMode::Host) {
        ImGui::Text("Room Host");
        ImGui::TextDisabled("Waiting for another player to join this room.");
        ImGui::Separator();
        ImGui::Text("Room code: %s", session.room_code.c_str());
        ImGui::Text("Listening on: 0.0.0.0 (all network interfaces)");
        ImGui::Text("LAN address: %s", local_ipv4_address().c_str());
        ImGui::Text("Port: %d", session.network_port);
        ImGui::TextDisabled("Give the LAN address and port to the other player.");
    } else if (session.multiplayer_mode == MultiplayerMode::Client) {
        ImGui::Text("Join Room");
        ImGui::TextDisabled("Connecting to the host and waiting for match setup.");
        ImGui::Separator();
        ImGui::Text("Room code: %s", session.room_code.c_str());
        ImGui::Text("Target host: %s", session.network_host.c_str());
        ImGui::Text("Port: %d", session.network_port);
    }

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.82f, 0.88f, 1.0f, 1.0f), "Status");
    ImGui::TextWrapped("%s", session.status_message.c_str());

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.72f, 0.91f, 0.78f, 1.0f), "Match readiness");
    ImGui::ProgressBar(session.network_ready ? 1.0f : 0.65f, ImVec2(-1.0f, 18.0f), session.network_ready ? "Opponent connected" : "Searching");

    if (session.network_ready) {
        if (ImGui::Checkbox("Ready", &session.local_ready)) {
            send_network_message(session, chess::build_ready_message(session.local_ready));
            session.status_message = session.local_ready ? "You are ready." : "You are not ready.";
        }
        ImGui::SameLine();
        ImGui::Text("Opponent: %s", session.opponent_ready ? "Ready" : "Not ready");

        if (session.multiplayer_mode == MultiplayerMode::Host && session.local_ready && session.opponent_ready &&
            ImGui::Button("Start Match", ImVec2(220.0f, 45.0f))) {
            reset_game_session(session);
            send_network_message(session, chess::build_match_start_message());
            broadcast_match_state(session, true);
            current_screen = Screen::Game;
        } else if (session.multiplayer_mode == MultiplayerMode::Host && (!session.local_ready || !session.opponent_ready)) {
            ImGui::TextDisabled("Both players must be ready before the host can start.");
        } else if (session.multiplayer_mode == MultiplayerMode::Client) {
            ImGui::TextDisabled("The host starts when both players are ready.");
        }
    }

    ImGui::Spacing();
    render_chat_panel(session, "WaitingRoomChat", 120.0f);

    ImGui::SameLine();
    if (ImGui::Button("Back to Lobby", ImVec2(180.0f, 45.0f))) {
        stop_network_session(session);
        current_screen = Screen::Lobby;
    }

    ImGui::End();
}

void render_game_screen(Screen& current_screen, GameSession& session) {
    if (session.is_multiplayer) {
        process_incoming_network_messages(session);
        broadcast_match_state(session);
    }

    if (session.is_game_over) {
        ImGui::OpenPopup("Game Over");
    }

    if (ImGui::BeginPopupModal("Game Over", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Game Over");
        ImGui::Separator();
        ImGui::Text("Winner: %s", color_name(session.winner_color).c_str());
        ImGui::TextWrapped("Checkmate was reached. Choose your next action.");
        ImGui::Spacing();

        if (ImGui::Button("Play Again", ImVec2(140.0f, 35.0f))) {
            reset_game_session(session);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Back to Lobby", ImVec2(140.0f, 35.0f))) {
            session.status_message = "Returned to lobby.";
            stop_network_session(session);
            current_screen = Screen::Lobby;
            session.is_game_over = false;
            session.winner_color = 0;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(1220.0f, 660.0f), ImGuiCond_Once);
    ImGui::Begin("Chess Engine");

    ImGui::Text(session.is_multiplayer ? "Multiplayer Match" : "Single Player Match");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", session.is_multiplayer ? "Network-based match ready" : "Local match");
    ImGui::Separator();

    ImGui::Columns(2, nullptr, false);

    ImGui::BeginChild("BoardPanel", ImVec2(560.0f, 0.0f), true);
    if (ImGui::BeginTable("BoardTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        const auto king_pos = find_king_position(session.board, session.current_color);
        const bool current_side_in_check = session.engine_director.is_king_in_check(session.engine_board, session.current_color);

        for (int row = 0; row < 8; ++row) {
            ImGui::TableNextRow();
            for (int col = 0; col < 8; ++col) {
                ImGui::TableSetColumnIndex(col);

                const bool black_square = (row + col) % 2 == 0;
                const bool selected = (row == session.selected_row && col == session.selected_col);
                const bool legal_target = std::find(session.legal_moves.begin(), session.legal_moves.end(), std::make_pair(row, col)) != session.legal_moves.end();
                const bool checked_king_square = current_side_in_check && row == king_pos.first && col == king_pos.second;
                ImVec4 cell_color = black_square ? ImVec4(0.90f, 0.88f, 0.82f, 1.0f)
                                                : ImVec4(0.56f, 0.48f, 0.35f, 1.0f);

                if (checked_king_square) {
                    cell_color = ImVec4(0.94f, 0.24f, 0.24f, 1.0f);
                } else if (selected) {
                    cell_color = ImVec4(0.94f, 0.80f, 0.22f, 1.0f);
                } else if (legal_target) {
                    const int target_value = session.board[row][col];
                    cell_color = (target_value == 0)
                        ? ImVec4(0.22f, 0.70f, 0.42f, 1.0f)
                        : ImVec4(0.84f, 0.28f, 0.22f, 1.0f);
                }

                ImGui::PushStyleColor(ImGuiCol_Button, cell_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, cell_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, cell_color);

                const int value = session.board[row][col];
                const std::string piece = piece_symbol(value);
                const char* label = piece.empty() ? " " : piece.c_str();

                ImGui::PushStyleColor(ImGuiCol_Text, value > 0 ? ImVec4(0.98f, 0.98f, 0.98f, 1.0f)
                                                            : ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
                ImGui::PushID(row * 8 + col);
                if (!session.is_game_over && ImGui::Button(label, ImVec2(52.0f, 52.0f))) {
                    sync_engine_board(session.engine_board, session.engine_pieces);
                    session.board = board_state_from_pieces(session.engine_pieces);

                    if (session.selected_row == -1 && session.selected_col == -1) {
                        pieces* clicked_piece = find_piece_at_position(session.engine_pieces, row, col);
                        if (clicked_piece != nullptr && session.engine_director.can_select(*clicked_piece) &&
                            (!session.is_multiplayer || clicked_piece->getcolor() == session.local_player_color)) {
                            session.selected_row = row;
                            session.selected_col = col;
                            session.legal_moves = collect_legal_moves_for_piece(session.engine_pieces, clicked_piece, session.engine_board);
                            session.status_message = session.legal_moves.empty() ? "No legal moves for this piece." : "Piece selected. Click a highlighted square.";
                        } else {
                            session.status_message = "It is not your turn to move that piece.";
                        }
                    } else if (session.selected_row == row && session.selected_col == col) {
                        session.selected_row = -1;
                        session.selected_col = -1;
                        session.legal_moves.clear();
                        session.status_message = "Selection cleared.";
                    } else if (std::find(session.legal_moves.begin(), session.legal_moves.end(), std::make_pair(row, col)) != session.legal_moves.end()) {
                        pieces* moving_piece = find_piece_at_position(session.engine_pieces, session.selected_row, session.selected_col);
                        if (moving_piece == nullptr) {
                            session.status_message = "No active piece selected.";
                        } else {
                            const vector from = engine_pos_from_ui(session.selected_row, session.selected_col);
                            const vector to = engine_pos_from_ui(row, col);
                            pieces* captured_piece = find_piece_at_position(session.engine_pieces, row, col);

                            const bool en_passant_capture = moving_piece->gettype() == 1 &&
                                std::abs(to.x - from.x) == 1 &&
                                !session.engine_board.has_piece(to) &&
                                session.engine_board.is_en_passant_target(to, moving_piece->getcolor());

                            if (session.engine_director.move_piece(*moving_piece, to.x, to.y, &session.engine_board)) {
                                if (en_passant_capture) {
                                    const vector captured_position{to.x, to.y - (moving_piece->getcolor() == 1 ? -1 : 1)};
                                    for (auto& piece : session.engine_pieces) {
                                        if (piece->is_alive() && piece->position().x == captured_position.x && piece->position().y == captured_position.y) {
                                            piece->set_alive(false);
                                            break;
                                        }
                                    }
                                    session.engine_board.remove_piece_at(captured_position);
                                }

                                if (captured_piece != nullptr && captured_piece != moving_piece) {
                                    captured_piece->set_alive(false);
                                    session.engine_board.remove_piece_at(to);
                                }
                                session.engine_board.move_piece(from, to);
                                sync_engine_board(session.engine_board, session.engine_pieces);

                                const int move_from_row = session.selected_row;
                                const int move_from_col = session.selected_col;
                                const int move_to_row = row;
                                const int move_to_col = col;

                                session.selected_row = -1;
                                session.selected_col = -1;
                                session.legal_moves.clear();
                                session.board = board_state_from_pieces(session.engine_pieces);
                                session.current_color = session.engine_director.get_current_color();
                                session.turn = static_cast<int>(session.engine_director.get_turn_count()) + 1;

                                if (session.is_multiplayer && session.network_ready && session.network_connection.socket_fd != INVALID_SOCKET) {
                                    const std::string move_message = build_remote_move_message(move_from_row, move_from_col, move_to_row, move_to_col, moving_piece->getcolor());
                                    send_network_message(session, move_message);
                                }

                                const int opponent_color = -session.current_color;
                                const bool opponent_in_check = session.engine_director.is_king_in_check(session.engine_board, opponent_color);
                                const bool opponent_checkmate = opponent_in_check && session.engine_director.is_checkmate_for(session.engine_board, opponent_color);
                                if (opponent_checkmate) {
                                    session.is_game_over = true;
                                    session.winner_color = -opponent_color;
                                    session.status_message = "Checkmate! " + color_name(session.winner_color) + " wins.";
                                } else {
                                    session.status_message = "Move applied.";
                                }
                            } else {
                                session.status_message = "Illegal move for this piece.";
                            }
                        }
                    } else {
                        pieces* clicked_piece = find_piece_at_position(session.engine_pieces, row, col);
                        if (clicked_piece != nullptr && session.engine_director.can_select(*clicked_piece) &&
                            (!session.is_multiplayer || clicked_piece->getcolor() == session.local_player_color)) {
                            session.selected_row = row;
                            session.selected_col = col;
                            session.legal_moves = collect_legal_moves_for_piece(session.engine_pieces, clicked_piece, session.engine_board);
                            session.status_message = session.legal_moves.empty() ? "No legal moves for this piece." : "Different piece selected.";
                        } else {
                            session.status_message = "Illegal move for this piece.";
                        }
                    }
                }
                ImGui::PopID();
                ImGui::PopStyleColor(4);
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::NextColumn();
    ImGui::BeginChild("StatusPanel", ImVec2(0.0f, 0.0f), true);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.96f, 0.98f, 1.0f));
    ImGui::TextColored(ImVec4(0.65f, 0.78f, 1.0f, 1.0f), "STATUS");
    ImGui::PopStyleColor();
    ImGui::Separator();

    std::chrono::milliseconds turn_elapsed = session.engine_director.get_turn_time();
    int board_score = session.engine_director.evaluate_board(session.engine_board);
    if (session.is_multiplayer && session.multiplayer_mode == MultiplayerMode::Client && session.has_remote_match_state) {
        const auto elapsed_since_sync = std::chrono::steady_clock::now() - session.last_remote_state_at;
        turn_elapsed = session.synced_turn_elapsed + std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_since_sync);
        board_score = session.synced_score;
    }
    const bool in_check = session.engine_director.is_king_in_check(session.engine_board, session.current_color);
    const bool kingside_castle = session.engine_board.can_castle(session.current_color, true);
    const bool queenside_castle = session.engine_board.can_castle(session.current_color, false);

    ImGui::TextColored(ImVec4(0.75f, 0.82f, 0.95f, 1.0f), "Turn");
    ImGui::SameLine(0.0f, 30.0f);
    ImGui::Text("%d", session.turn);

    ImGui::TextColored(ImVec4(0.75f, 0.82f, 0.95f, 1.0f), "Color");
    ImGui::SameLine(0.0f, 24.0f);
    ImGui::TextColored(session.current_color == 1 ? ImVec4(0.98f, 0.98f, 0.98f, 1.0f) : ImVec4(0.18f, 0.18f, 0.18f, 1.0f), "%s", color_name(session.current_color).c_str());

    ImGui::TextColored(ImVec4(0.75f, 0.82f, 0.95f, 1.0f), "Timer");
    ImGui::SameLine(0.0f, 22.0f);
    ImGui::Text("%s", format_turn_timer(turn_elapsed).c_str());

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.72f, 0.92f, 0.78f, 1.0f), "Score");
    ImGui::SameLine(0.0f, 30.0f);
    ImGui::Text("%s", format_score_value(board_score).c_str());

    const float score_bar_value = std::clamp((board_score + 1200.0f) / 2400.0f, 0.0f, 1.0f);
    const ImVec4 score_bar_color = board_score >= 0 ? ImVec4(0.45f, 0.76f, 0.55f, 1.0f)
                                                   : ImVec4(0.79f, 0.39f, 0.39f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f, 0.20f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, score_bar_color);
    ImGui::ProgressBar(score_bar_value, ImVec2(-1.0f, 16.0f), board_score >= 0 ? "White favor" : "Black favor");
    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    if (session.is_game_over) {
        ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Status: Game Over");
    } else if (in_check) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Status: Check");
    } else {
        ImGui::TextColored(ImVec4(0.43f, 0.78f, 0.53f, 1.0f), "Status: Normal");
    }

    if (kingside_castle || queenside_castle) {
        ImGui::TextColored(ImVec4(0.35f, 0.82f, 0.6f, 1.0f), "Castling: %s%s",
            kingside_castle ? "K" : "",
            queenside_castle ? "Q" : "");
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.96f, 0.8f, 0.52f, 1.0f), "Message");
    ImGui::TextWrapped("%s", session.status_message.c_str());

    if (session.is_multiplayer) {
        ImGui::Separator();
        render_chat_panel(session, "GameChat", 120.0f);
    }

    ImGui::Separator();
    if (session.selected_row >= 0 && session.selected_col >= 0) {
        const int value = session.board[session.selected_row][session.selected_col];
        ImGui::TextColored(ImVec4(0.78f, 0.85f, 0.97f, 1.0f), "Selected");
        ImGui::Text("(%d, %d)", session.selected_row, session.selected_col);
        if (value != 0) {
            ImGui::Text("Piece: %s", piece_symbol(value).c_str());
            ImGui::Text("Type: %d", std::abs(value));
            ImGui::Text("Color: %s", value > 0 ? "White" : "Black");
        } else {
            ImGui::Text("Empty square");
        }
    } else {
        ImGui::TextColored(ImVec4(0.78f, 0.85f, 0.97f, 1.0f), "Selected: none");
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.82f, 0.86f, 0.98f, 1.0f), "Controls");
    if (ImGui::Button("New Game", ImVec2(140.0f, 35.0f))) {
        reset_game_session(session);
    }

    ImGui::SameLine();
    if (ImGui::Button("Lobby", ImVec2(140.0f, 35.0f))) {
        session.status_message = "Returned to lobby.";
        stop_network_session(session);
        current_screen = Screen::Lobby;
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(140.0f, 35.0f))) {
        reset_game_session(session);
    }

    ImGui::Separator();
    ImGui::TextWrapped("Current board and turn state follow the internal engine rules, and piece selection and movement are handled by clicking the board.");
    ImGui::EndChild();

    ImGui::Columns(1);
    ImGui::End();
}

}  // namespace

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << '\n';
}

int main() {
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Chess Engine GUI", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    Screen current_screen = Screen::Start;
    bool should_close = false;
    GameSession session;
    reset_game_session(session);

    while (!glfwWindowShouldClose(window) && !should_close) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        switch (current_screen) {
            case Screen::Start:
                render_start_screen(current_screen, should_close);
                break;
            case Screen::Lobby:
                render_lobby_screen(current_screen);
                break;
            case Screen::ModeSelect:
                render_mode_select_screen(current_screen, session);
                break;
            case Screen::WaitingRoom:
                render_waiting_room_screen(current_screen, session);
                break;
            case Screen::Game:
                render_game_screen(current_screen, session);
                break;
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.08f, 0.09f, 0.10f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    stop_network_session(session);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
