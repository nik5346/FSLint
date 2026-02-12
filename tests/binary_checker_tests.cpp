#include "binary_parser.h"
#include "certificate.h"
#include "fmi2_binary_checker.h"
#include "fmi3_binary_checker.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

TEST_CASE("Binary Parser with Reference FMUs", "[binary][reference]")
{
    if (!fs::exists("tests/reference_fmus"))
    {
        FAIL("Reference FMUs not found. Please run scripts/download_reference_fmus.sh first.");
    }

    SECTION("ELF (Linux)")
    {
        fs::path path = "tests/reference_fmus/fmi2/BouncingBall/binaries/linux64/BouncingBall.so";
        REQUIRE(fs::exists(path));
        auto exports = BinaryParser::getExports(path);
        CHECK(exports.contains("fmi2Instantiate"));
        CHECK(exports.contains("fmi2GetVersion"));
    }

    SECTION("PE (Windows)")
    {
        fs::path path = "tests/reference_fmus/fmi2/BouncingBall/binaries/win64/BouncingBall.dll";
        REQUIRE(fs::exists(path));
        auto exports = BinaryParser::getExports(path);
        CHECK(exports.contains("fmi2Instantiate"));
    }

    SECTION("Mach-O (macOS)")
    {
        fs::path path = "tests/reference_fmus/fmi2/BouncingBall/binaries/darwin64/BouncingBall.dylib";
        REQUIRE(fs::exists(path));
        auto exports = BinaryParser::getExports(path);
        CHECK(exports.contains("fmi2Instantiate"));
    }
}

TEST_CASE("Binary Checker with Reference FMUs", "[binary][checker]")
{
    if (!fs::exists("tests/reference_fmus"))
    {
        FAIL("Reference FMUs not found. Please run scripts/download_reference_fmus.sh first.");
    }

    SECTION("FMI 2.0 BouncingBall")
    {
        fs::path path = "tests/reference_fmus/fmi2/BouncingBall";
        REQUIRE(fs::exists(path));
        Fmi2BinaryChecker checker;
        Certificate cert;
        checker.validate(path, cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("FMI 3.0 BouncingBall")
    {
        fs::path path = "tests/reference_fmus/fmi3/BouncingBall";
        REQUIRE(fs::exists(path));
        Fmi3BinaryChecker checker;
        Certificate cert;
        checker.validate(path, cert);
        CHECK_FALSE(has_fail(cert));
    }
}
