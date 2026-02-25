#include "binary_parser.h"
#include "certificate.h"
#include "fmi1_binary_checker.h"
#include "fmi2_binary_checker.h"
#include "fmi3_binary_checker.h"
#include "model_checker.h"
#include "test_helpers.h"
#include "zipper.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static bool reference_fmus_available()
{
    static bool available = fs::exists("tests/reference_fmus/1.0/cs/BouncingBall.fmu") &&
                            fs::exists("tests/reference_fmus/2.0/BouncingBall.fmu") &&
                            fs::exists("tests/reference_fmus/3.0/BouncingBall.fmu");
    if (!available)
    {
        static bool warned = false;
        if (!warned)
        {
            std::cerr << "[WARNING] Reference FMUs not found. Binary tests will be skipped.\n";
            std::cerr << "          Run 'scripts/download_reference_fmus.py' to download them.\n";
            warned = true;
        }
    }
    return available;
}

TEST_CASE("Binary Parser ELF", "[binary][elf]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    fs::path temp_bin = "tests/temp_elf.so";
    {
        Zipper zipper;
        REQUIRE(zipper.open("tests/reference_fmus/2.0/BouncingBall.fmu"));
        std::vector<uint8_t> content;
        REQUIRE(zipper.extractFile("binaries/linux64/BouncingBall.so", content));
        std::ofstream outfile(temp_bin, std::ios::binary);
        outfile.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    }

    auto exports = BinaryParser::getExports(temp_bin);
    CHECK(exports.contains("fmi2Instantiate"));
    fs::remove(temp_bin);
}

TEST_CASE("Binary Parser PE", "[binary][pe]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    fs::path temp_bin = "tests/temp_pe.dll";
    {
        Zipper zipper;
        REQUIRE(zipper.open("tests/reference_fmus/2.0/BouncingBall.fmu"));
        std::vector<uint8_t> content;
        REQUIRE(zipper.extractFile("binaries/win64/BouncingBall.dll", content));
        std::ofstream outfile(temp_bin, std::ios::binary);
        outfile.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    }

    auto exports = BinaryParser::getExports(temp_bin);
    CHECK(exports.contains("fmi2Instantiate"));
    fs::remove(temp_bin);
}

TEST_CASE("Binary Parser Mach-O", "[binary][macho]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    fs::path temp_bin = "tests/temp_macho.dylib";
    {
        Zipper zipper;
        REQUIRE(zipper.open("tests/reference_fmus/2.0/BouncingBall.fmu"));
        std::vector<uint8_t> content;
        REQUIRE(zipper.extractFile("binaries/darwin64/BouncingBall.dylib", content));
        std::ofstream outfile(temp_bin, std::ios::binary);
        outfile.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    }

    auto exports = BinaryParser::getExports(temp_bin);
    CHECK(exports.contains("fmi2Instantiate"));
    fs::remove(temp_bin);
}

TEST_CASE("FMI 1.0 Binary Exports", "[binary][fmi1]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    SECTION("CS")
    {
        ModelChecker mc;
        Certificate cert = mc.validate("tests/reference_fmus/1.0/cs/BouncingBall.fmu", true);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("ME")
    {
        ModelChecker mc;
        Certificate cert = mc.validate("tests/reference_fmus/1.0/me/BouncingBall.fmu", true);
        CHECK_FALSE(has_fail(cert));
    }
}

TEST_CASE("FMI 3.0 Binary Exports", "[binary][fmi3]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    ModelChecker mc;
    Certificate cert = mc.validate("tests/reference_fmus/3.0/BouncingBall.fmu", true);
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

    // Extract FMI 1.0 binary and rename it to match modelIdentifier 'test'
    {
        Zipper zipper;
        REQUIRE(zipper.open("tests/reference_fmus/1.0/cs/BouncingBall.fmu"));
        std::vector<uint8_t> content;
        REQUIRE(zipper.extractFile("binaries/linux64/BouncingBall.so", content));
        std::ofstream outfile(binaries_dir / "test.so", std::ios::binary);
        outfile.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    }

    Fmi2BinaryChecker checker;
    Certificate cert;
    checker.validate(temp_path, cert);

    CHECK(has_fail(cert));
    CHECK(has_error_with_text(cert, "Mandatory function 'fmi2GetVersion' is not exported."));

    // Cleanup
    fs::remove_all(temp_path);
}

TEST_CASE("FMI 1.0 Binary Validation Failure", "[binary][fmi1][checker]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    // Create a temporary test dir
    fs::path temp_path = "tests/fmi1_binary_fail";
    fs::create_directories(temp_path);

    // Write a minimal FMI 1.0 modelDescription.xml
    std::ofstream md_file(temp_path / "modelDescription.xml");
    md_file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            << "<fmiModelDescription fmiVersion=\"1.0\" modelName=\"fail_test\" modelIdentifier=\"fail_test\" "
               "guid=\"{123}\">\n"
            << "  <ModelVariables/>\n"
            << "</fmiModelDescription>\n";
    md_file.close();

    // Create binaries dir
    fs::path binaries_dir = temp_path / "binaries" / "linux64";
    fs::create_directories(binaries_dir);

    // Extract BouncingBall.so (which has BouncingBall_ prefixes) and save it as fail_test.so
    // The checker will expect fail_test_ prefixes and thus fail.
    {
        Zipper zipper;
        REQUIRE(zipper.open("tests/reference_fmus/1.0/cs/BouncingBall.fmu"));
        std::vector<uint8_t> content;
        REQUIRE(zipper.extractFile("binaries/linux64/BouncingBall.so", content));
        std::ofstream outfile(binaries_dir / "fail_test.so", std::ios::binary);
        outfile.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    }

    Fmi1BinaryChecker checker;
    Certificate cert;
    checker.validate(temp_path, cert);

    CHECK(has_fail(cert));
    CHECK(has_error_with_text(cert, "Mandatory function 'fail_test_fmiGetVersion' is not exported."));

    // Cleanup
    fs::remove_all(temp_path);
}
