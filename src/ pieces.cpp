#include "vector.cpp"
#include "linked_list.cpp"

class pieces {
public:
    // color: -1 white, 1 black
    // type: 1 pawn, 2 knight, 3 bishop, 4 rook, 5 queen, 6 king
    pieces(int piece_color, int piece_type, vector start_position)
        : color(piece_color), type(piece_type), nowposition(start_position) {}

    bool move(int x, int y) {
        vector destination{x, y};

        if (!is_valid_move(destination)) {
            return false;
        }

        pathlist.clear();
        add_path(destination);
        nowposition = destination;
        ++turncount;
        return true;
    }

    vector position() const {
        return nowposition;
    }

    const linked_list& path() const {
        return pathlist;
    }

private:
    int color = 0;
    int type = 0;
    int turncount = 0;
    linked_list pathlist;
    vector nowposition{0, 0};

    bool is_valid_move(vector destination) const {
        int dx = destination.x - nowposition.x;
        int dy = destination.y - nowposition.y;
        int distance_x = absolute(dx);
        int distance_y = absolute(dy);

        if (!is_on_board(destination) || (dx == 0 && dy == 0)) {
            return false;
        }

        switch (type) {
            case 1: {
                int direction = color == -1 ? 1 : -1;

                if (dy == 0 && dx == direction) {
                    return true;
                }

                return dy == 0 && dx == direction * 2 && turncount == 0;
            }
            case 2:
                return  (distance_x == 2 && distance_y == 1) ||
                        (distance_x == 1 && distance_y == 2);
            case 3:
                return distance_x == distance_y;
            case 4:
                return dx == 0 || dy == 0;
            case 5:
                return distance_x == distance_y || dx == 0 || dy == 0;
            case 6:
                return distance_x <= 1 && distance_y <= 1;
            default:
                return false;
        }
    }

    static int absolute(int value) {
        return value < 0 ? -value : value;
    }

    static bool is_on_board(vector position) {
        return  position.x >= 0 && position.x < 8 &&
                position.y >= 0 && position.y < 8;
    }

    void add_path(vector destination) {
        int dx = destination.x - nowposition.x;
        int dy = destination.y - nowposition.y;
        int step_x = dx == 0 ? 0 : (dx > 0 ? 1 : -1);
        int step_y = dy == 0 ? 0 : (dy > 0 ? 1 : -1);
        int steps = absolute(dx) > absolute(dy) ? absolute(dx) : absolute(dy);

        for (int step = 1; step < steps; ++step) {
            pathlist.push_back(vector{
                nowposition.x + step_x * step,
                nowposition.y + step_y * step
            });
        }
    }
};