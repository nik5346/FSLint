#include "model_checker.h"
#include "certificate.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("Detect unknown model type as failure", "[detection]")
{
    ModelChecker checker;
    fs::path dummy_dir = "tests/data/directory/fail/not_a_model";

    Certificate cert = checker.validate(dummy_dir, true);

    CHECK(has_fail(cert));
}

TEST_CASE("Detect unknown model type as failure in addCertificate", "[detection]")
{
    ModelChecker checker;
    fs::path dummy_dir = "tests/data/directory/fail/not_a_model";

    bool result = checker.addCertificate(dummy_dir);

    CHECK_FALSE(result);
}
