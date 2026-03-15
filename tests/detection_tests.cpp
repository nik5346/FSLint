#include "certificate.h"
#include "model_checker.h"
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

TEST_CASE("Detect unknown model type as failure with .fmu extension", "[detection]")
{
    ModelChecker checker;
    fs::path dummy_dir = "tests/data/directory/fail/not_a_model.fmu";

    Certificate cert = checker.validate(dummy_dir, true);

    // It should still fail because modelDescription.xml is missing,
    // but detection should have succeeded based on extension.
    CHECK(has_fail(cert));
    CHECK(has_error_with_text(cert, "modelDescription.xml"));
}

TEST_CASE("Detect unknown model type as failure in addCertificate", "[detection]")
{
    ModelChecker checker;
    fs::path dummy_dir = "tests/data/directory/fail/not_a_model";

    bool result = checker.addCertificate(dummy_dir);

    CHECK_FALSE(result);
}
