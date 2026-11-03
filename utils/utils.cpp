#include "utils.hpp"

void logging(const std::string& message, const std::string& level) {
    std::cout << "[" << level << "] " << message << std::endl;
}