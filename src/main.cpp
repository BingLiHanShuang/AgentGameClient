#include "engine.h"
#include <filesystem>
#include <iostream>

int main() {
    const std::string game_root = "G0";
    if (!std::filesystem::exists(game_root)) {
        std::cerr << "[Error] Game directory not found.\n";
        return 1;
    }
    try {
        GalGameEngine engine(game_root);
        engine.run();
    } catch (const std::exception& e) {
        std::cerr << "[Fatal] " << e.what() << "\n";
        return 1;
    }
    return 0;
}
