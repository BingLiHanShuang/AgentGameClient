#include "engine.h"
#include <filesystem>
#include <iostream>

int main() {
    const std::string story_dir = "Story";
    if (!std::filesystem::exists(story_dir)) {
        std::cerr << "[Error] Story directory not found.\n";
        return 1;
    }
    try {
        GalGameEngine engine(story_dir);
        engine.run();
    } catch (const std::exception& e) {
        std::cerr << "[Fatal] " << e.what() << "\n";
        return 1;
    }
    return 0;
}
