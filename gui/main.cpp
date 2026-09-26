#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../src/game/bord.cpp"
#include "../src/manager/Director.cpp"
#include "../src/object/pieces.cpp"

namespace {

using BoardState = std::array<std::array<int, 8>, 8>;

enum class Screen {
    Start,
    Lobby,
    ModeSelect,
    Game
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
};

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
    session.status_message = session.is_multiplayer ? "Multiplayer match ready." : "Single-player match ready.";
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
    ImGui::Text("- Multiplayer: room create / join setup");
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
    ImGui::TextDisabled("Single-player uses local game logic, multiplayer is prepared for network gameplay integration.");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Start Single Player", ImVec2(270.0f, 90.0f))) {
        session.is_multiplayer = false;
        reset_game_session(session);
        current_screen = Screen::Game;
    }

    ImGui::SameLine();
    if (ImGui::Button("Start Multiplayer", ImVec2(270.0f, 90.0f))) {
        session.is_multiplayer = true;
        reset_game_session(session);
        current_screen = Screen::Game;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginChild("ModeInfo", ImVec2(0.0f, 150.0f), true);
    ImGui::Text("Current Setup");
    ImGui::Text("- Single Player: local board and turn logic");
    ImGui::Text("- Multiplayer: prepared for future network connection");
    ImGui::Text("- Next goal: room creation, queue, and opponent turn sync");
    ImGui::EndChild();

    ImGui::Spacing();
    if (ImGui::Button("Back to Lobby", ImVec2(200.0f, 40.0f))) {
        current_screen = Screen::Lobby;
    }

    ImGui::End();
}

void render_game_screen(Screen& current_screen, GameSession& session) {
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
        for (int row = 0; row < 8; ++row) {
            ImGui::TableNextRow();
            for (int col = 0; col < 8; ++col) {
                ImGui::TableSetColumnIndex(col);

                const bool black_square = (row + col) % 2 == 0;
                const bool selected = (row == session.selected_row && col == session.selected_col);
                const bool legal_target = std::find(session.legal_moves.begin(), session.legal_moves.end(), std::make_pair(row, col)) != session.legal_moves.end();
                ImVec4 cell_color = black_square ? ImVec4(0.75f, 0.74f, 0.69f, 1.0f)
                                                : ImVec4(0.44f, 0.43f, 0.35f, 1.0f);

                if (selected) {
                    cell_color = ImVec4(0.92f, 0.74f, 0.22f, 1.0f);
                } else if (legal_target) {
                    const int target_value = session.board[row][col];
                    cell_color = (target_value == 0)
                        ? ImVec4(0.24f, 0.68f, 0.36f, 1.0f)
                        : ImVec4(0.82f, 0.31f, 0.27f, 1.0f);
                }

                ImGui::PushStyleColor(ImGuiCol_Button, cell_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, cell_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, cell_color);

                const int value = session.board[row][col];
                const std::string piece = piece_symbol(value);
                const char* label = piece.empty() ? " " : piece.c_str();

                ImGui::PushID(row * 8 + col);
                if (!session.is_game_over && ImGui::Button(label, ImVec2(52.0f, 52.0f))) {
                    sync_engine_board(session.engine_board, session.engine_pieces);
                    session.board = board_state_from_pieces(session.engine_pieces);

                    if (session.selected_row == -1 && session.selected_col == -1) {
                        pieces* clicked_piece = find_piece_at_position(session.engine_pieces, row, col);
                        if (clicked_piece != nullptr && session.engine_director.can_select(*clicked_piece)) {
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

                                session.selected_row = -1;
                                session.selected_col = -1;
                                session.legal_moves.clear();
                                session.board = board_state_from_pieces(session.engine_pieces);
                                session.current_color = session.engine_director.get_current_color();
                                session.turn = static_cast<int>(session.engine_director.get_turn_count()) + 1;

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
                        if (clicked_piece != nullptr && session.engine_director.can_select(*clicked_piece)) {
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
                ImGui::PopStyleColor(3);
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::NextColumn();
    ImGui::BeginChild("StatusPanel", ImVec2(0.0f, 0.0f), true);

    ImGui::Text("Turn: %d", session.turn);
    ImGui::Text("Current color: %s", color_name(session.current_color).c_str());

    const auto turn_elapsed = session.engine_director.get_turn_time();
    ImGui::Text("Turn timer: %s", format_turn_timer(turn_elapsed).c_str());

    const int board_score = session.engine_director.evaluate_board(session.engine_board);
    ImGui::Text("Score: %s", format_score_value(board_score).c_str());
    ImGui::Text("Board evaluation: %s", format_score_value(board_score).c_str());

    const bool in_check = session.engine_director.is_king_in_check(session.engine_board, session.current_color);
    const bool checkmate = in_check && session.engine_director.is_checkmate_for(session.engine_board, session.current_color);
    const bool kingside_castle = session.engine_board.can_castle(session.current_color, true);
    const bool queenside_castle = session.engine_board.can_castle(session.current_color, false);
    if (session.is_game_over) {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Status: Game Over");
    } else if (in_check) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Status: Check");
    } else {
        ImGui::Text("Status: Normal");
    }

    if (kingside_castle || queenside_castle) {
        ImGui::TextColored(ImVec4(0.25f, 0.8f, 0.45f, 1.0f), "Castling: %s%s",
            kingside_castle ? "K" : "",
            queenside_castle ? "Q" : "");
    }

    ImGui::Separator();
    ImGui::TextWrapped("%s", session.status_message.c_str());

    if (session.selected_row >= 0 && session.selected_col >= 0) {
        const int value = session.board[session.selected_row][session.selected_col];
        ImGui::Text("Selected: (%d, %d)", session.selected_row, session.selected_col);
        if (value != 0) {
            ImGui::Text("Piece: %s", piece_symbol(value).c_str());
            ImGui::Text("Type: %d", std::abs(value));
            ImGui::Text("Color: %s", value > 0 ? "White" : "Black");
        } else {
            ImGui::Text("Empty square");
        }
    } else {
        ImGui::Text("Selected: none");
    }

    ImGui::Separator();
    ImGui::Text("Game Controls");
    if (ImGui::Button("New Game", ImVec2(140.0f, 35.0f))) {
        reset_game_session(session);
    }

    ImGui::SameLine();
    if (ImGui::Button("Lobby", ImVec2(140.0f, 35.0f))) {
        session.status_message = "Returned to lobby.";
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

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
