#include "binary_parser.h"
#include "certificate.h"
#include "fmi1_binary_checker.h"
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
    static bool available = fs::exists("tests/reference_fmus/BouncingBall_10") &&
                            fs::exists("tests/reference_fmus/BouncingBall_20") &&
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

    auto exports =
        BinaryParser::getExports("tests/reference_fmus/BouncingBall_20/binaries/darwin64/BouncingBall.dylib");
    CHECK(exports.contains("fmi2Instantiate"));
}

TEST_CASE("FMI 1.0 Binary Exports", "[binary][fmi1]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    SECTION("ME")
    {
        Fmi1BinaryChecker checker;
        Certificate cert;
        checker.validate("tests/reference_fmus/BouncingBall_10", cert);
        CHECK_FALSE(has_fail(cert));
    }
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

TEST_CASE("Binary Checker Validation Failure", "[binary][checker]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    // Use FMI 1.0 binaries with FMI 2.0 checker to test export validation failure.
    // FMI 1.0 functions do not have the '2' prefix (e.g., fmiInstantiate vs fmi2Instantiate).

    // We need a path that looks like an FMU root with modelDescription.xml
    // but contains FMI 1.0 binaries.

    // Create a temporary test dir
    fs::path temp_path = "tests/binary_checker_fail_test";
    fs::create_directories(temp_path);

    // Copy modelDescription.xml from a FMI 2.0 pass case
    fs::copy_file("tests/data/fmi2/pass/dist_binaries_only/modelDescription.xml", temp_path / "modelDescription.xml",
                  fs::copy_options::overwrite_existing);

    // Create binaries dir
    fs::path binaries_dir = temp_path / "binaries" / "linux64";
    fs::create_directories(binaries_dir);

    // Copy FMI 1.0 binary and rename it to match modelIdentifier 'test'
    fs::copy_file("tests/reference_fmus/BouncingBall_10/binaries/linux64/BouncingBall.so", binaries_dir / "test.so",
                  fs::copy_options::overwrite_existing);

    Fmi2BinaryChecker checker;
    Certificate cert;
    checker.validate(temp_path, cert);

    CHECK(has_fail(cert));
    CHECK(has_error_with_text(cert, "Mandatory function 'fmi2GetVersion' is not exported."));

    // Cleanup
    fs::remove_all(temp_path);
}
