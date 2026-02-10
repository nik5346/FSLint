#include "fmi3_directory_checker.h"
#include "certificate.h"
#include <set>

void Fmi3DirectoryChecker::performVersionSpecificChecks(
    const std::filesystem::path& path, Certificate& cert, const std::map<std::string, std::string>& model_identifiers,
    [[maybe_unused]] const std::set<std::string>& listed_sources_in_md)
{
    TestResult test{"FMU Structure", TestStatus::PASS, {}};

    static const std::set<std::string> fmi3_standard_entries = {
        "modelDescription.xml", "documentation", "terminalsAndIcons", "sources", "binaries", "resources", "extra"};

    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        std::string name = entry.path().filename().string();
        if (!fmi3_standard_entries.contains(name))
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("Unknown entry in FMU root: '" + name + "'.");
        }
    }

    // FMI 3 documentation/ check
    if (std::filesystem::exists(path / "documentation"))
    {
        if (std::filesystem::exists(path / "documentation" / "diagram.svg") &&
            !std::filesystem::exists(path / "documentation" / "diagram.png"))
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back(
                "FMI 3.0: diagram.svg exists in documentation/ but diagram.png is missing (required "
                "if diagram.svg is provided).");
        }
    }

    // FMI 3 terminalsAndIcons/ check
    if (std::filesystem::exists(path / "terminalsAndIcons"))
    {
        if (std::filesystem::exists(path / "terminalsAndIcons" / "icon.svg") &&
            !std::filesystem::exists(path / "terminalsAndIcons" / "icon.png"))
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("FMI 3.0: icon.svg exists in terminalsAndIcons/ but icon.png is missing (required "
                                    "if icon.svg is provided).");
        }
    }

    // Distribution check
    bool has_binaries = false;
    if (std::filesystem::exists(path / "binaries"))
    {
        for (const auto& entry : std::filesystem::directory_iterator(path / "binaries"))
        {
            if (entry.is_directory())
            {
                for (const auto& [interface, model_id] : model_identifiers)
                {
                    for (const auto& ext : {".dll", ".so", ".dylib"})
                    {
                        if (std::filesystem::exists(entry.path() / (model_id + ext)))
                        {
                            has_binaries = true;
                            break;
                        }
                    }
                    if (has_binaries)
                        break;
                }
            }
            if (has_binaries)
                break;
        }
    }

    bool has_sources = std::filesystem::exists(path / "sources" / "buildDescription.xml") ||
                       (std::filesystem::exists(path / "sources") && !std::filesystem::is_empty(path / "sources"));

    if (!has_binaries && !has_sources)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back(
            "FMU must contain either a precompiled binary for at least one platform or source code.");
    }

    cert.printTestResult(test);
    cert.printSubsectionSummary(test.status != TestStatus::FAIL);
}
