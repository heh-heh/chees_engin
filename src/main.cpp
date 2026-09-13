#include <stdio.h>
#include <iostream>
#include "game/game.cpp"
using namespace std;

void testlog(string logtext);
gamemanager game;

int main(void){
    game.startgame();

    return 0;
}

void testlog(string logtext){
    cout << logtext ;
}