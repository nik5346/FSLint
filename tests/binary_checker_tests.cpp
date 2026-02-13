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

static bool reference_fmus_available()
{
    static bool available = fs::exists("tests/reference_fmus/BouncingBall_20") &&
                            fs::exists("tests/reference_fmus/BouncingBall_30");
    if (!available)
    {
        static bool warned = false;
        if (!warned)
        {
            std::cerr << "[WARNING] Reference FMUs not found. Binary tests will be skipped.\n";
            std::cerr << "          Run 'scripts/download_reference_fmus.sh' to download them.\n";
            warned = true;
        }
    }
    return available;
}

TEST_CASE("Binary Parser ELF", "[binary][elf]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    auto exports = BinaryParser::getExports("tests/reference_fmus/BouncingBall_20/binaries/linux64/BouncingBall.so");
    CHECK(exports.contains("fmi2Instantiate"));
}

TEST_CASE("Binary Parser PE", "[binary][pe]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    auto exports = BinaryParser::getExports("tests/reference_fmus/BouncingBall_20/binaries/win64/BouncingBall.dll");
    CHECK(exports.contains("fmi2Instantiate"));
}

TEST_CASE("Binary Parser Mach-O", "[binary][macho]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    auto exports = BinaryParser::getExports("tests/reference_fmus/BouncingBall_20/binaries/darwin64/BouncingBall.dylib");
    CHECK(exports.contains("fmi2Instantiate"));
}

TEST_CASE("FMI 3.0 Binary Exports", "[binary][fmi3]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    Fmi3BinaryChecker checker;
    Certificate cert;
    checker.validate("tests/reference_fmus/BouncingBall_30", cert);
    CHECK_FALSE(has_fail(cert));
}

TEST_CASE("Binary Checker Validation", "[binary][checker]")
{
    // This test uses a dynamically created dummy so it doesn't need reference FMUs
    std::string path = "tests/data/binary/fail_exports";
    fs::path binary_dir = fs::path(path) / "binaries" / "linux64";
    fs::create_directories(binary_dir);
    fs::path binary_file = binary_dir / "Test.so";

    std::ofstream ofs(binary_file);
    ofs << "NOT A REAL BINARY";
    ofs.close();

    Fmi2BinaryChecker checker;
    Certificate cert;
    checker.validate(path, cert);

    CHECK(has_fail(cert));
    CHECK(has_error_with_text(cert, "Mandatory function 'fmi2GetVersion' is not exported."));

    // Cleanup
    fs::remove(binary_file);
}
