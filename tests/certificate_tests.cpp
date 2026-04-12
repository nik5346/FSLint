#include "certificate.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Certificate reporting logic", "[certificate]")
{
    Certificate cert;

    SECTION("Initial state")
    {
        CHECK_FALSE(cert.isFailed());
    }

    SECTION("Single test failure")
    {
        cert.printSubsectionHeader("Group 1");
        cert.printTestResult({"Test 1", TestStatus::FAIL, {"Failure"}});
        CHECK(cert.isFailed());

        cert.printSubsectionSummary(true); // Incorrectly passed true
        CHECK(cert.getFullReport().find("Result: FAILED") != std::string::npos);
    }

    SECTION("Multiple subsections, first one fails")
    {
        cert.printSubsectionHeader("Group 1");
        cert.printTestResult({"Test 1", TestStatus::FAIL, {"Failure"}});
        cert.printSubsectionSummary(false);

        cert.printSubsectionHeader("Group 2");
        cert.printTestResult({"Test 2", TestStatus::PASS, {}});
        cert.printSubsectionSummary(true);

        CHECK(cert.isFailed());
        cert.printFooter();
        CHECK(cert.getFullReport().find("MODEL VALIDATION FAILED") != std::string::npos);
    }

    SECTION("Subsection valid parameter override")
    {
        cert.printSubsectionHeader("Group 1");
        cert.printTestResult({"Test 1", TestStatus::PASS, {}});
        cert.printSubsectionSummary(false); // Manually failed but should not set isFailed()

        CHECK_FALSE(cert.isFailed());
        CHECK(cert.getFullReport().find("Result: FAILED") != std::string::npos);
    }

    SECTION("Multiple messages in test result")
    {
        cert.printSubsectionHeader("Group 1");
        cert.printTestResult({"Test 1", TestStatus::FAIL, {"Message 1", "Message 2", "Message 3"}});
        const std::string report = cert.getFullReport();
        CHECK(report.find("      ├─ Message 1") != std::string::npos);
        CHECK(report.find("      ├─ Message 2") != std::string::npos);
        CHECK(report.find("      └─ Message 3") != std::string::npos);
    }

    SECTION("Pass results are included without messages")
    {
        cert.printSubsectionHeader("Group 1");
        cert.printTestResult({"Test Pass", TestStatus::PASS, {"Message 1"}});
        const std::string report = cert.getFullReport();
        CHECK(report.find("[✓ PASS] Test Pass") != std::string::npos);
        CHECK(report.find("Message 1") == std::string::npos);
    }
}
