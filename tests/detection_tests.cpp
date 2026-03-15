#include "model_checker.h"
#include "certificate.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("Detect unknown model type as failure", "[detection]")
{
    ModelChecker checker;
    fs::path dummy_dir = "dummy_model_dir";
    fs::create_directories(dummy_dir);

    // Create some random file
    {
        std::ofstream f(dummy_dir / "random.txt");
        f << "not an fmu";
    }

    Certificate cert = checker.validate(dummy_dir, true);

    CHECK(has_fail(cert));

    fs::remove_all(dummy_dir);
}

TEST_CASE("Detect unknown model type as failure in addCertificate", "[detection]")
{
    ModelChecker checker;
    fs::path dummy_dir = "dummy_model_dir_cert";
    fs::create_directories(dummy_dir);

    // Create some random file
    {
        std::ofstream f(dummy_dir / "random.txt");
        f << "not an fmu";
    }

    bool result = checker.addCertificate(dummy_dir);

    CHECK_FALSE(result);

    fs::remove_all(dummy_dir);
}
