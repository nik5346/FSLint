#include "certificate.h"
#include "file_utils.h"
#include "fmi2_terminals_and_icons_checker.h"
#include "fmi3_terminals_and_icons_checker.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("FMI 2.0 Terminals and Icons Validation", "[terminals][icons][fmi2]")
{
    Fmi2TerminalsAndIconsChecker checker;

    auto validate_pass = [&](const std::string& path)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        CHECK_FALSE(has_fail(cert));
    };

    auto validate_fail = [&](const std::string& path, const std::string& expected_error)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (!has_error_with_text(cert, expected_error))
        {
            UNSCOPED_INFO("Expected error '" << expected_error << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.status == TestStatus::FAIL)
                {
                    UNSCOPED_INFO("  FAIL: " << res.test_name);
                    for (const auto& msg : res.messages)
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        REQUIRE(has_fail(cert));
        CHECK(has_error_with_text(cert, expected_error));
    };

    SECTION("Passing Cases")
    {
        validate_pass("tests/data/fmi2/terminals_and_icons/pass");
    }

    SECTION("Failure Cases")
    {
        validate_fail("tests/data/fmi2/terminals_and_icons/fail/non_existent_var", "references non-existent variable");
        validate_fail("tests/data/fmi2/terminals_and_icons/fail/illegal_causality",
                      "must have causality 'input', 'output', 'parameter', or 'calculatedParameter'");
        validate_fail("tests/data/fmi2/terminals_and_icons/fail/duplicate_terminal", "is not unique at its level");
        validate_fail("tests/data/fmi2/terminals_and_icons/fail/duplicate_member", "is not unique in terminal");
    }
}

TEST_CASE("FMI 3.0 Terminals and Icons Validation", "[terminals][icons][fmi3]")
{
    Fmi3TerminalsAndIconsChecker checker;

    auto validate_pass = [&](const std::string& path)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        CHECK_FALSE(has_fail(cert));
    };

    auto validate_fail = [&](const std::string& path, const std::string& expected_error)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (!has_error_with_text(cert, expected_error))
        {
            UNSCOPED_INFO("Expected error '" << expected_error << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.status == TestStatus::FAIL)
                {
                    UNSCOPED_INFO("  FAIL: " << res.test_name);
                    for (const auto& msg : res.messages)
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        REQUIRE(has_fail(cert));
        CHECK(has_error_with_text(cert, expected_error));
    };

    SECTION("Passing Cases")
    {
        validate_pass("tests/data/fmi3/terminals_and_icons/pass");
        validate_pass("tests/data/fmi3/terminals_and_icons/pass/missing_member_name_sequence");
    }

    SECTION("Failure Cases")
    {
        validate_fail("tests/data/fmi3/terminals_and_icons/fail/stream_flow_constraint",
                      "has multiple inflow/outflow variables and a stream variable");
        validate_fail("tests/data/fmi3/terminals_and_icons/fail/duplicate_stream_member",
                      "Stream member name \"h\" is not unique");
        validate_fail("tests/data/fmi3/terminals_and_icons/fail/illegal_stream_causality",
                      "must have causality 'output' or 'calculatedParameter'");
        validate_fail("tests/data/fmi3/terminals_and_icons/fail/missing_member_name_plug",
                      "is missing mandatory 'memberName' for matchingRule 'plug'");
        validate_fail("tests/data/fmi3/terminals_and_icons/fail/missing_member_name_bus",
                      "is missing mandatory 'memberName' for matchingRule 'bus'");
        validate_fail("tests/data/fmi3/terminals_and_icons/fail/mismatched_stream_dims", "has mismatched dimensions");
        validate_fail("tests/data/fmi3/terminals_and_icons/fail/missing_icon_png", "not found");
        validate_fail("tests/data/fmi3/terminals_and_icons/fail/invalid_icon_uri",
                      "must be a relative URI and must not contain \"..\"");
        validate_fail("tests/data/fmi3/terminals_and_icons/fail/invalid_connection_color",
                      "must have exactly 3 RGB values");
    }
}
