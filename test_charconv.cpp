#include <charconv>
#include <system_error>
#include <iostream>

int main() {
    double val;
    std::string s = "1.23";
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
    if (ec == std::errc()) {
        std::cout << "Success: " << val << std::endl;
    } else {
        std::cout << "Failure" << std::endl;
    }
    return 0;
}
