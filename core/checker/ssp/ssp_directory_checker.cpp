#include "ssp_directory_checker.h"

#include "certificate.h"

#include <filesystem>

void SspDirectoryChecker::validate(const std::filesystem::path& path, Certificate& cert)
{
    cert.printSubsectionHeader("SSP DIRECTORY STRUCTURE");

    // Documentation Entry Point
    {
        TestResult test{"Documentation Entry Point", TestStatus::PASS, {}};
        if (!std::filesystem::exists(path / "documentation" / "index.html"))
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Recommended entry point 'documentation/index.html' is missing.");
        }
        cert.printTestResult(test);
    }

    cert.printSubsectionSummary(true);
}
