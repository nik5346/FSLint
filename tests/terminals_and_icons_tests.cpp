#include "certificate.h"
#include "terminals_and_icons_checker.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

namespace
{
bool has_fail(const Certificate& cert)
{
    const auto& results = cert.getResults();
    return std::any_of(results.begin(), results.end(),
                       [](const TestResult& r) { return r.status == TestStatus::FAIL; });
}

bool has_error_with_text(const Certificate& cert, const std::string& text)
{
    const auto& results = cert.getResults();
    for (const auto& r : results)
    {
        if (r.status != TestStatus::FAIL)
            continue;
        if (r.test_name.find(text) != std::string::npos)
            return true;
        for (const auto& msg : r.messages)
            if (msg.find(text) != std::string::npos)
                return true;
    }
    return false;
}
} // namespace

TEST_CASE("FMI 2.0 Terminals and Icons Validation", "[terminals][icons][fmi2]")
{
    Fmi2TerminalsAndIconsChecker checker;

    auto validate_pass = [&](const std::string& path)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << path);
        CHECK_FALSE(has_fail(cert));
    };

    auto validate_fail = [&](const std::string& path, const std::string& expected_error)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << path);
        REQUIRE(has_fail(cert));
        CHECK(has_error_with_text(cert, expected_error));
    };

    SECTION("Pass")
    {
        validate_pass("tests/data/fmi2/terminals_and_icons/pass");
    }

    SECTION("Failures")
    {
        validate_fail("tests/data/fmi2/terminals_and_icons/fail/version_mismatch",
                      "fmiVersion in terminalsAndIcons.xml");
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
        INFO("Checking path: " << path);
        CHECK_FALSE(has_fail(cert));
    };

    auto validate_fail = [&](const std::string& path, const std::string& expected_error)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << path);
        REQUIRE(has_fail(cert));
        CHECK(has_error_with_text(cert, expected_error));
    };

    SECTION("Pass")
    {
        validate_pass("tests/data/fmi3/terminals_and_icons/pass");
    }

    SECTION("Failures")
    {
        validate_fail("tests/data/fmi3/terminals_and_icons/fail/version_mismatch",
                      "must match fmiVersion in modelDescription.xml");
        validate_fail("tests/data/fmi3/terminals_and_icons/fail/stream_flow_constraint",
                      "has multiple inflow/outflow variables and a stream variable");
        validate_fail("tests/data/fmi3/terminals_and_icons/fail/duplicate_stream_member",
                      "Stream member name \"h\" is not unique");
        validate_fail("tests/data/fmi3/terminals_and_icons/fail/illegal_stream_causality",
                      "must have causality 'output' or 'calculatedParameter'");
    }
}
