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

    auto info = BinaryParser::parse(temp_bin);
    CHECK(info.format == BinaryFormat::ELF);
    REQUIRE(info.architectures.size() == 1);
    CHECK(info.architectures[0].bitness == 64);
    CHECK(info.isSharedLibrary);
    CHECK(info.exports.contains("fmi2Instantiate"));
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

    auto info = BinaryParser::parse(temp_bin);
    CHECK(info.format == BinaryFormat::PE);
    REQUIRE(info.architectures.size() == 1);
    CHECK(info.architectures[0].bitness == 64);
    CHECK(info.isSharedLibrary);
    CHECK(info.exports.contains("fmi2Instantiate"));
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

    auto info = BinaryParser::parse(temp_bin);
    CHECK(info.format == BinaryFormat::MACHO);
    REQUIRE(info.architectures.size() == 1);
    CHECK(info.architectures[0].bitness == 64);
    CHECK(info.isSharedLibrary);
    CHECK(info.exports.contains("fmi2Instantiate"));
    fs::remove(temp_bin);
}

TEST_CASE("FMI 1.0 Binary Exports", "[binary][fmi1]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    SECTION("CS")
    {
        Fmi1BinaryChecker checker;
        Certificate cert;
        checker.validate("tests/reference_fmus/1.0/cs/BouncingBall.fmu", cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("ME")
    {
        Fmi1BinaryChecker checker;
        Certificate cert;
        checker.validate("tests/reference_fmus/1.0/me/BouncingBall.fmu", cert);
        CHECK_FALSE(has_fail(cert));
    }
}

TEST_CASE("FMI 3.0 Binary Exports", "[binary][fmi3]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    Fmi3BinaryChecker checker;
    Certificate cert;
    checker.validate("tests/reference_fmus/3.0/BouncingBall.fmu", cert);
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

TEST_CASE("Binary Format Mismatch Failure", "[binary][checker][format]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    // Create a temporary test dir
    fs::path temp_path = "tests/binary_format_fail_test";
    fs::create_directories(temp_path);

    // Copy modelDescription.xml from a FMI 2.0 pass case
    fs::copy_file("tests/data/fmi2/pass/dist_binaries_only/modelDescription.xml", temp_path / "modelDescription.xml",
                  fs::copy_options::overwrite_existing);

    // Create binaries dir for Windows, but put an ELF (Linux) binary there.
    fs::path binaries_dir = temp_path / "binaries" / "win64";
    fs::create_directories(binaries_dir);

    {
        Zipper zipper;
        REQUIRE(zipper.open("tests/reference_fmus/2.0/BouncingBall.fmu"));
        std::vector<uint8_t> content;
        // Extract Linux binary
        REQUIRE(zipper.extractFile("binaries/linux64/BouncingBall.so", content));
        // Save it as test.dll in win64 folder
        std::ofstream outfile(binaries_dir / "test.dll", std::ios::binary);
        outfile.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    }

    Fmi2BinaryChecker checker;
    Certificate cert;
    checker.validate(temp_path, cert);

    CHECK(has_fail(cert));
    CHECK(has_error_with_text(cert, "Binary format is not PE (Windows), found ELF"));

    // Cleanup
    fs::remove_all(temp_path);
}

TEST_CASE("Binary Bitness Mismatch Failure", "[binary][checker][bitness]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    // Create a temporary test dir
    fs::path temp_path = "tests/binary_bitness_fail_test";
    fs::create_directories(temp_path);

    // Copy modelDescription.xml from a FMI 2.0 pass case
    fs::copy_file("tests/data/fmi2/pass/dist_binaries_only/modelDescription.xml", temp_path / "modelDescription.xml",
                  fs::copy_options::overwrite_existing);

    // Create binaries dir for 32-bit Linux, but put a 64-bit ELF binary there.
    fs::path binaries_dir = temp_path / "binaries" / "linux32";
    fs::create_directories(binaries_dir);

    {
        Zipper zipper;
        REQUIRE(zipper.open("tests/reference_fmus/2.0/BouncingBall.fmu"));
        std::vector<uint8_t> content;
        // Extract 64-bit Linux binary
        REQUIRE(zipper.extractFile("binaries/linux64/BouncingBall.so", content));
        // Save it in linux32 folder
        std::ofstream outfile(binaries_dir / "test.so", std::ios::binary);
        outfile.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    }

    Fmi2BinaryChecker checker;
    Certificate cert;
    checker.validate(temp_path, cert);

    CHECK(has_fail(cert));
    CHECK(has_error_with_text(cert, "Binary does not contain a 32-bit x86 architecture matching platform 'linux32'."));

    // Cleanup
    fs::remove_all(temp_path);
}

TEST_CASE("Shared Library Check Failure", "[binary][checker][shared]")
{
    // Create a temporary test dir
    fs::path temp_path = "tests/binary_shared_fail_test";
    fs::create_directories(temp_path);

    fs::copy_file("tests/data/fmi2/pass/dist_binaries_only/modelDescription.xml", temp_path / "modelDescription.xml",
                  fs::copy_options::overwrite_existing);

    fs::path binaries_dir = temp_path / "binaries" / "linux64";
    fs::create_directories(binaries_dir);

    // Create a dummy non-shared library ELF file.
    // ELF Header for 64-bit, little endian, ET_EXEC (executable) instead of ET_DYN.
    std::vector<uint8_t> elf_header = {
        0x7f, 'E', 'L', 'F', 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, // e_ident
        2,    0,                                                 // e_type = ET_EXEC
        62,   0,                                                 // e_machine = EM_X86_64
        1,    0,   0,   0,                                       // e_version
        0,    0,   0,   0,   0, 0, 0, 0,                         // e_entry
        0,    0,   0,   0,   0, 0, 0, 0,                         // e_phoff
        0,    0,   0,   0,   0, 0, 0, 0,                         // e_shoff
        0,    0,   0,   0,                                       // e_flags
        64,   0,                                                 // e_ehsize
        56,   0,                                                 // e_phentsize
        0,    0,                                                 // e_phnum
        64,   0,                                                 // e_shentsize
        0,    0,                                                 // e_shnum
        0,    0                                                  // e_shstrndx
    };

    std::ofstream outfile(binaries_dir / "test.so", std::ios::binary);
    outfile.write(reinterpret_cast<const char*>(elf_header.data()), static_cast<std::streamsize>(elf_header.size()));
    outfile.close();

    Fmi2BinaryChecker checker;
    Certificate cert;
    checker.validate(temp_path, cert);

    CHECK(has_fail(cert));
    CHECK(has_error_with_text(cert, "Binary is not a shared library (DLL/SO/DYLIB)."));

    fs::remove_all(temp_path);
}

TEST_CASE("FMI 1.0 Binary Validation Failure", "[binary][fmi1][checker]")
{
    if (!reference_fmus_available())
        SKIP("Reference FMUs not available");

    // Create a temporary test dir
    fs::path temp_path = "tests/fmi1_binary_fail";
    fs::create_directories(temp_path);

    // Use an existing valid FMI 1.0 modelDescription.xml (modelIdentifier="TestME")
    fs::copy_file("tests/data/fmi1/fail/binary_invalid_prefix/modelDescription.xml", temp_path / "modelDescription.xml",
                  fs::copy_options::overwrite_existing);

#if defined(_WIN32)
    std::string platform = "win64";
    std::string ext = ".dll";
#elif defined(__APPLE__)
    std::string platform = "darwin64";
    std::string ext = ".dylib";
#else
    std::string platform = "linux64";
    std::string ext = ".so";
#endif

    // Create binaries dir
    fs::path binaries_dir = temp_path / "binaries" / platform;
    fs::create_directories(binaries_dir);

    // Extract BouncingBall binary and save it as TestME with host extension
    // The BouncingBall binary has BouncingBall_ prefixes, but the checker will expect TestME_ prefixes.
    {
        Zipper zipper;
        REQUIRE(zipper.open("tests/reference_fmus/1.0/cs/BouncingBall.fmu"));
        std::vector<uint8_t> content;
        REQUIRE(zipper.extractFile("binaries/" + platform + "/BouncingBall" + ext, content));
        std::ofstream outfile(binaries_dir / ("TestME" + ext), std::ios::binary);
        outfile.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    }

    Fmi1BinaryChecker checker;
    Certificate cert;
    checker.validate(temp_path, cert);

    CHECK(has_fail(cert));
    CHECK(has_error_with_text(cert, "Mandatory function 'TestME_fmiGetVersion' is not exported."));

    // Cleanup
    fs::remove_all(temp_path);
}
