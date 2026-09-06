#include <iostream>
#include <iomanip>
#include "vector.cpp"

#ifndef CHESS_BOARD_H
#define CHESS_BOARD_H

class Board {
    public :
        Board(){
        }
        void printbord(const vector* selected = nullptr){
            for(int i=0;i<8;i++){
                for(int j=0;j<8;j++){
                    bool is_selected = selected != nullptr &&
                                       selected->x == i && selected->y == j;
                    if (is_selected) {
                        std::cout << "\033[43;30m";
                    }
                    std::cout << std::setw(2) << map[i][j];
                    if (is_selected) {
                        std::cout << "\033[0m";
                    }
                    std::cout << "  ";
                }
                std::cout << std::endl;
                std::cout << std::endl;
            }
        }
        void move_piece(vector from, vector to){
            map[to.x][to.y] = map[from.x][from.y];
            map[from.x][from.y] = 0;
        }
    private :
        int map[8][8]={
            {4, 2, 3, 5, 6, 3, 2, 4},
            {1, 1, 1, 1, 1, 1, 1, 1},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {-1,-1,-1,-1,-1,-1,-1,-1},
            {-4,-2,-3,-5,-6,-3,-2,-4}
        };
};

#endif