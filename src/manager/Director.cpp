#include <chrono>
#include <cstddef>
#include "../object/pieces.cpp"
#include "../datas/vector.cpp"

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

	bool is_king_in_check(const Board& board, int color) const {
		return board.is_king_in_check(color);
	}

	bool is_checkmate_for(const Board& board, int color) const {
		return board.is_checkmate_for(color);
	}

	int evaluate_board(const Board& board) const {
		int total_score = 0;
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				const vector position{x, y};
				if (!board.has_piece(position)) {
					continue;
				}

				const int piece_color = board.get_piece_color_at(position);
				const int piece_type = board.get_piece_type_at(position);
				const int base_value = piece_value(piece_type);
				const int positional_bonus = positional_bonus_for(piece_type, position);
				const int evaluate_value = base_value + positional_bonus;

				total_score += (piece_color == 1) ? evaluate_value : -evaluate_value;
			}
		}

		if (current_color == 1) {
			total_score += 5;
		} else {
			total_score -= 5;
		}

		return total_score;
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
	static int piece_value(int piece_type) {
		switch (piece_type) {
			case 1: return 100;
			case 2: return 320;
			case 3: return 330;
			case 4: return 500;
			case 5: return 900;
			case 6: return 20000;
			default: return 0;
		}
	}

	static int positional_bonus_for(int piece_type, const vector& position) {
		const int center_distance = std::max(std::abs(position.x - 3), std::abs(position.y - 3));
		const int centrality = 3 - center_distance;
		const int bonus = std::max(0, centrality);

		switch (piece_type) {
			case 1: return bonus * 4;
			case 2: return bonus * 8;
			case 3: return bonus * 10;
			case 4: return bonus * 6;
			case 5: return bonus * 12;
			case 6: return bonus * 4;
			default: return 0;
		}
	}

	std::size_t turn_count = 0;
	int current_color = 1;
	std::chrono::steady_clock::time_point turn_start;
};