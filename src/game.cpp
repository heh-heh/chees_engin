#include <iostream>
#include "bord.cpp"

class gamemanager{
    public:
        void startgame(){
        }
        void updategame();
        void rendergame(){
            board.printbord();
        }
        void endgame();
    private:
        Board board;

};