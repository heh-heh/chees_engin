#include <iostream>
#include <iomanip>
class Board {
    public :
        Board(){
        }
        void printbord(){
            for(int i=0;i<8;i++){
                for(int j=0;j<8;j++){
                    std::cout << std::setw(2) << map[i][j] << "  ";
                }
                std::cout << std::endl;
                std::cout << std::endl;
            }
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