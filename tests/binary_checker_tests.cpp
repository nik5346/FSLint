#include "binary_parser.h"
#include "certificate.h"
#include "fmi2_binary_checker.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace fs = std::filesystem;

/* Binary files removed - disabling tests
TEST_CASE("Binary Parser ELF", "[binary][elf]")
{
    auto exports = BinaryParser::getExports("tests/data/binary/elf/libdummy.so");
    CHECK(exports.contains("exported_func"));
}

TEST_CASE("Binary Parser PE", "[binary][pe]")
{
    auto exports = BinaryParser::getExports("tests/data/binary/pe/test.dll");
    CHECK(exports.contains("fmi2Instantiate"));
}

TEST_CASE("Binary Parser Mach-O", "[binary][macho]")
{
    auto exports = BinaryParser::getExports("tests/data/binary/macho/test.dylib");
    CHECK(exports.contains("fmi2Instantiate"));
}
*/

TEST_CASE("Binary Checker Validation", "[binary][checker]")
{
    Fmi2BinaryChecker checker;
    Certificate cert;
    checker.validate("tests/data/binary/fail_exports", cert);

    CHECK(has_fail(cert));
    CHECK(has_error_with_text(cert, "Mandatory function 'fmi2GetVersion' is not exported."));
}
