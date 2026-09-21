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

BoardState make_initial_board() {
    BoardState board{};
    const int backrank[8] = {4, 2, 3, 5, 6, 3, 2, 4};

    for (int x = 0; x < 8; ++x) {
        board[0][x] = backrank[x];
        board[1][x] = 1;
        board[6][x] = -1;
        board[7][x] = -backrank[x];
    }

    return board;
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

std::vector<std::pair<int, int>> collect_legal_moves_for_piece(
    const std::vector<std::unique_ptr<pieces>>& piece_list,
    pieces* selected_piece,
    const Board& board_state) {
    (void)piece_list;
    std::vector<std::pair<int, int>> moves;
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const vector destination = engine_pos_from_ui(row, col);
            if (selected_piece->can_move_to(destination, &board_state)) {
                moves.emplace_back(row, col);
            }
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
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    Board engine_board;
    std::vector<std::unique_ptr<pieces>> engine_pieces;
    Director engine_director;
    initialize_engine_pieces(engine_pieces);
    sync_engine_board(engine_board, engine_pieces);

    BoardState board = board_state_from_pieces(engine_pieces);
    int turn = 1;
    int current_color = engine_director.get_current_color();
    int selected_row = -1;
    int selected_col = -1;
    std::vector<std::pair<int, int>> legal_moves;
    std::string status_message = "Select a piece to move.";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(1220.0f, 660.0f), ImGuiCond_Once);
        ImGui::Begin("Chess Engine");

        ImGui::Text("Chess Engine GUI Prototype");
        ImGui::Separator();

        ImGui::Columns(2, nullptr, false);

        ImGui::BeginChild("BoardPanel", ImVec2(560.0f, 0.0f), true);
        if (ImGui::BeginTable("BoardTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            for (int row = 0; row < 8; ++row) {
                ImGui::TableNextRow();
                for (int col = 0; col < 8; ++col) {
                    ImGui::TableSetColumnIndex(col);

                    const bool black_square = (row + col) % 2 == 0;
                    const bool selected = (row == selected_row && col == selected_col);
                    const bool legal_target = std::find(legal_moves.begin(), legal_moves.end(), std::make_pair(row, col)) != legal_moves.end();
                    ImVec4 cell_color = black_square ? ImVec4(0.75f, 0.74f, 0.69f, 1.0f)
                                                    : ImVec4(0.44f, 0.43f, 0.35f, 1.0f);

                    if (selected) {
                        cell_color = ImVec4(0.92f, 0.74f, 0.22f, 1.0f);
                    } else if (legal_target) {
                        const int target_value = board[row][col];
                        cell_color = (target_value == 0)
                            ? ImVec4(0.24f, 0.68f, 0.36f, 1.0f)
                            : ImVec4(0.82f, 0.31f, 0.27f, 1.0f);
                    }

                    ImGui::PushStyleColor(ImGuiCol_Button, cell_color);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, cell_color);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, cell_color);

                    const int value = board[row][col];
                    const std::string piece = piece_symbol(value);
                    const char* label = piece.empty() ? " " : piece.c_str();

                    ImGui::PushID(row * 8 + col);
                    if (ImGui::Button(label, ImVec2(52.0f, 52.0f))) {
                        sync_engine_board(engine_board, engine_pieces);
                        board = board_state_from_pieces(engine_pieces);

                        if (selected_row == -1 && selected_col == -1) {
                            pieces* clicked_piece = find_piece_at_position(engine_pieces, row, col);
                            if (clicked_piece != nullptr && engine_director.can_select(*clicked_piece)) {
                                selected_row = row;
                                selected_col = col;
                                legal_moves = collect_legal_moves_for_piece(engine_pieces, clicked_piece, engine_board);
                                status_message = legal_moves.empty() ? "No legal moves for this piece." : "Piece selected. Click a highlighted square.";
                            } else {
                                status_message = "It is not your turn to move that piece.";
                            }
                        } else if (selected_row == row && selected_col == col) {
                            selected_row = -1;
                            selected_col = -1;
                            legal_moves.clear();
                            status_message = "Selection cleared.";
                        } else if (std::find(legal_moves.begin(), legal_moves.end(), std::make_pair(row, col)) != legal_moves.end()) {
                            pieces* moving_piece = find_piece_at_position(engine_pieces, selected_row, selected_col);
                            if (moving_piece == nullptr) {
                                status_message = "No active piece selected.";
                            } else {
                                const vector from = engine_pos_from_ui(selected_row, selected_col);
                                const vector to = engine_pos_from_ui(row, col);
                                pieces* captured_piece = find_piece_at_position(engine_pieces, row, col);

                                if (engine_director.move_piece(*moving_piece, to.x, to.y, &engine_board)) {
                                    if (captured_piece != nullptr && captured_piece != moving_piece) {
                                        captured_piece->set_alive(false);
                                        engine_board.remove_piece_at(to);
                                    }
                                    engine_board.move_piece(from, to);
                                    sync_engine_board(engine_board, engine_pieces);

                                    selected_row = -1;
                                    selected_col = -1;
                                    legal_moves.clear();
                                    board = board_state_from_pieces(engine_pieces);
                                    current_color = engine_director.get_current_color();
                                    turn = static_cast<int>(engine_director.get_turn_count()) + 1;
                                    status_message = "Move applied.";
                                } else {
                                    status_message = "Illegal move for this piece.";
                                }
                            }
                        } else {
                            pieces* clicked_piece = find_piece_at_position(engine_pieces, row, col);
                            if (clicked_piece != nullptr && engine_director.can_select(*clicked_piece)) {
                                selected_row = row;
                                selected_col = col;
                                legal_moves = collect_legal_moves_for_piece(engine_pieces, clicked_piece, engine_board);
                                status_message = legal_moves.empty() ? "No legal moves for this piece." : "Different piece selected.";
                            } else {
                                status_message = "Illegal move for this piece.";
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

        ImGui::Text("Turn: %d", turn);
        ImGui::Text("Current color: %s", color_name(current_color).c_str());
        ImGui::Separator();
        ImGui::TextWrapped("%s", status_message.c_str());

        if (selected_row >= 0 && selected_col >= 0) {
            const int value = board[selected_row][selected_col];
            ImGui::Text("Selected: (%d, %d)", selected_row, selected_col);
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
        if (ImGui::Button("Reset Board")) {
            initialize_engine_pieces(engine_pieces);
            engine_board = Board{};
            sync_engine_board(engine_board, engine_pieces);
            engine_director = Director{};
            board = board_state_from_pieces(engine_pieces);
            selected_row = -1;
            selected_col = -1;
            legal_moves.clear();
            turn = 1;
            current_color = engine_director.get_current_color();
            status_message = "Board reset.";
        }

        ImGui::SameLine();
        if (ImGui::Button("Next Turn")) {
            turn += 1;
            current_color = current_color == 1 ? -1 : 1;
        }

        ImGui::TextWrapped("This panel is connected to the project's engine board logic and updates from the same piece/turn state used by the chess rules.");
        ImGui::EndChild();

        ImGui::Columns(1);
        ImGui::End();

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
