#include "certificate.h"
#include "model_checker.h"
#include "test_helpers.h"
#include "zipper.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

// Subclass ModelChecker to override ArchiveChecker with a mock
class MockModelChecker : public ModelChecker
{
  public:
    Certificate validate(const std::filesystem::path& path, bool quiet, bool show_tree,
                         Certificate cert) const override
    {
        (void)path;
        (void)show_tree;
        cert.setQuiet(quiet);

        // Mock Step 1: Archive validation
        cert.printSubsectionHeader("ARCHIVE VALIDATION");
        cert.printTestResult({"[SECURITY] Mock Security Issue", TestStatus::FAIL, {"Failed for testing."}});
        cert.printSubsectionSummary(false);

        if (cert.shouldAbort())
        {
            if (!quiet)
                cert.printFooter();
            return cert;
        }

        // Mock Step 2 & 3: Just print Subsection to show we continued
        cert.printSubsectionHeader("MODEL DETECTION");
        cert.printTestResult({"Model Detection", TestStatus::PASS, {}});
        cert.printSubsectionSummary(true);

        return cert;
    }
};

TEST_CASE("Security Continuation", "[core][security]")
{
    const std::filesystem::path test_file = "dummy.txt";
    {
        std::ofstream f(test_file);
        f << "dummy";
    }

    const MockModelChecker validator;

    SECTION("Abort on security issue (default/cancel)")
    {
        Certificate cert;
        cert.setContinueCallback([](const TestResult&) { return false; });

        const Certificate result = validator.validate(test_file, true, false, std::move(cert));

        CHECK(result.isFailed());
        CHECK(result.shouldAbort());
        CHECK(result.getFullReport().find("MODEL DETECTION") == std::string::npos);
    }

    SECTION("Continue after security issue")
    {
        Certificate cert;
        cert.setContinueCallback([](const TestResult&) { return true; });

        const Certificate result = validator.validate(test_file, true, false, std::move(cert));

        CHECK(result.isFailed());
        CHECK_FALSE(result.shouldAbort());
        CHECK(result.getFullReport().find("MODEL DETECTION") != std::string::npos);
    }

    if (std::filesystem::exists(test_file))
        std::filesystem::remove(test_file);
}
