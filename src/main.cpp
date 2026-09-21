#include <cstdlib>
#include <iostream>
#include <string>

int main(void) {
    std::cout << "Launching GUI chess board..." << std::endl;

    const std::string gui_path = "./build/chess_gui";
    const int exit_code = std::system(gui_path.c_str());
    return exit_code;
}