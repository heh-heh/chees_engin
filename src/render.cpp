void gamemanager::rendergame(bool reset_cursor){
    std::lock_guard<std::mutex> lock(display_mutex);
    rendergame_unlocked(reset_cursor);
}

void gamemanager::rendergame_unlocked(bool reset_cursor,
                                      const vector* selected,
                                      const std::vector<vector>* legal_moves,
                                      const std::vector<vector>* blocked_moves){
    if (reset_cursor) {
        std::cout << "\033[2J\033[H";
    }
    render_status();
    std::cout << "\033[2;1H";
    board.printbord(selected, legal_moves, blocked_moves);
}

void gamemanager::render_status(){
    // for(int i=0; i<pieces2.size(); i++){
    //     std::cout << "type : " << pieces2[i].
    // }

    std::cout << "\033[s\033[1;1H";
    std::cout << "Turn: " << director.get_turn_count()
              << ", Current color: "
              << (director.get_current_color() == 1 ? "White" : "Black")
              << ", Elapsed time: "
              << director.get_turn_time().count() / 1000.0 << "s   ";
    std::cout << "\033[u" << std::flush;
}
