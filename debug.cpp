#include "checker_factory.h"
#include "certificate.h"
#include <iostream>
#include <filesystem>

int main() {
    Fmi2DirectoryChecker checker;
    Certificate cert;
    checker.validate("tests/data/fmi2/pass/dist_both", cert);
    for (const auto& res : cert.getResults()) {
        std::cout << "[" << (int)res.status << "] " << res.test_name << std::endl;
        for (const auto& msg : res.messages) {
            std::cout << "  - " << msg << std::endl;
        }
    }
    return 0;
}
