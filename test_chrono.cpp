#include <chrono>
#include <iostream>
#include <sstream>

int main() {
    std::string dt = "2024-08-21T12:15:13";
    std::istringstream is{dt};
    std::chrono::local_seconds ltp;
    if (is >> std::chrono::parse("%FT%T", ltp)) {
        std::cout << "Parsed: " << dt << std::endl;
    } else {
        std::cout << "Failed to parse" << std::endl;
    }
    return 0;
}
