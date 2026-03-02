#include "certificate.h"
#include "fmi1_schema_checker.h"
#include "fmi2_schema_checker.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("FMI 1.0 Encoding Validation", "[fmi1][encoding]")
{
    Fmi1MeSchemaChecker checker;

    SECTION("ISO-8859-1 Warning")
    {
        Certificate cert;
        checker.validate("tests/data/fmi1/warn/encoding_iso", cert);
        CHECK(has_warning_with_text(cert, "It is recommended to use UTF-8"));
        CHECK_FALSE(has_fail(cert));
    }
}

TEST_CASE("FMI 2.0 Encoding Validation", "[fmi2][encoding]")
{
    Fmi2SchemaChecker checker;

    SECTION("ISO-8859-1 Failure")
    {
        Certificate cert;
        checker.validate("tests/data/fmi2/fail/encoding_iso", cert);
        CHECK(has_error_with_text(cert, "Encoding must be UTF-8, found: ISO-8859-1"));
        CHECK(has_fail(cert));
    }

    SECTION("Invalid UTF-8 Content Failure")
    {
        Certificate cert;
        checker.validate("tests/data/fmi2/fail/encoding_invalid_content", cert);
        CHECK(has_error_with_text(cert, "File content is not valid UTF-8"));
        CHECK(has_fail(cert));
    }
}
