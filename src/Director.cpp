#include <chrono>
#include <cstddef>
#include "pieces.cpp"
#include "vector.cpp"

class Director{
public:
	Director()
		: turn_start(std::chrono::steady_clock::now()) {}

	bool can_select(const pieces& piece) const {
		return piece.getcolor() == current_color;
	}

	bool move_piece(pieces& piece, int x, int y, const Board* board = nullptr) {
		if (piece.getcolor() != current_color || !piece.move(x, y, board)) {
			return false;
		}

		++turn_count;
		current_color = -current_color;
		turn_start = std::chrono::steady_clock::now();
		return true;
	}

	std::size_t get_turn_count() const {
		return turn_count;
	}

	int get_current_color() const {
		return current_color;
	}

	std::chrono::milliseconds get_turn_time() const {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - turn_start);
	}

private:
	std::size_t turn_count = 0;
	int current_color = 1;
	std::chrono::steady_clock::time_point turn_start;
};