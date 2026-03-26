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
        cert.printSubsectionSummary(false); // Manually failed

        CHECK(cert.isFailed());
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

    SECTION("Suppress PASS results")
    {
        cert.printSubsectionHeader("Group 1");
        cert.printTestResult({"Test 1", TestStatus::PASS, {"This should not appear"}});
        const std::string report = cert.getFullReport();
        CHECK(report.find("[✓ PASS]") == std::string::npos);
        CHECK(report.find("Test 1") == std::string::npos);
        CHECK(report.find("This should not appear") == std::string::npos);

        // Nested models
        cert.addNestedModelResult({"Nested PASS", TestStatus::PASS, {}});
        cert.addNestedModelResult({"Nested FAIL", TestStatus::FAIL, {}});
        cert.printNestedModelsTree();
        const std::string report2 = cert.getFullReport();
        CHECK(report2.find("Nested PASS") == std::string::npos);
        CHECK(report2.find("Nested FAIL") != std::string::npos);
    }
}
