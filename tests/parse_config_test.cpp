#include "config_parser.h"
#include <iostream>

int main() {
    try {
        Config cfg = parse_config("tests/rtsp_config.json");
        if (cfg.mode != InputMode::RTSP) {
            std::cerr << "Wrong mode" << std::endl;
            return 1;
        }
        std::cout << "Parsed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
