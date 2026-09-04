#include <stdio.h>
#include <iostream>
using namespace std;

void testlog(string logtext);

int main(void){
    testlog("test\n");

    return 0;
}

void testlog(string logtext){
    cout << logtext ;
}