void gamemanager::updategame(){//업데이트
    int from_x;
    int from_y;
    int to_x;
    int to_y;

    std::cout << "Enter the coordinates of the piece to select (fx fy): " << std::flush;
    if (!(std::cin >> from_x >> from_y)) {
        endgame = true;
        return;
    }

    if (!is_on_board(from_x, from_y)) {
        std::cout << "Invalid coordinates.\n";
        return;
    }

    pieces* selected_piece = find_piece_at(from_x, from_y);

    if (selected_piece == nullptr) {
        std::cout << "No piece exists at that position.\n";
        return;
    }

    if (!director.can_select(*selected_piece)) {
        std::cout << "That piece belongs to the other side.\n";
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

    std::cout << "Enter destination coordinates (tx ty): " << std::flush;
    if (!(std::cin >> to_x >> to_y)) {
        endgame = true;
        return;
    }

    if (!is_on_board(to_x, to_y)) {
        std::cout << "Invalid coordinates.\n";
        return;
    }

    if (!board.is_path_clear(selected_position, vector{to_x, to_y}, selected_piece->gettype())) {
        std::cout << "Movement is blocked by an obstacle in that direction.\n";
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
        std::cout << "Move is not allowed.\n";
    }
};
