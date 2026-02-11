#include "fmi3_directory_checker.h"
#include "certificate.h"
#include <set>

void Fmi3DirectoryChecker::performVersionSpecificChecks(
    const std::filesystem::path& path, Certificate& cert, const std::map<std::string, std::string>& model_identifiers,
    [[maybe_unused]] const std::set<std::string>& listed_sources_in_md)
{
    // 1. FMU Root Entries
    {
        TestResult test{"FMU Root Entries", TestStatus::PASS, {}};

        static const std::set<std::string> fmi3_standard_entries = {
            "modelDescription.xml", "documentation", "terminalsAndIcons", "sources", "binaries", "resources", "extra"};

        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            std::string name = entry.path().filename().string();
            if (!fmi3_standard_entries.contains(name))
            {
                test.status = TestStatus::WARNING;
                test.messages.push_back("Unknown entry in FMU root: '" + name + "'.");
            }
        }
        cert.printTestResult(test);
    }

    // 2. Documentation Files
    {
        TestResult test{"Documentation Files", TestStatus::PASS, {}};
        if (std::filesystem::exists(path / "documentation"))
        {
            if (std::filesystem::exists(path / "documentation" / "diagram.svg") &&
                !std::filesystem::exists(path / "documentation" / "diagram.png"))
            {
                test.status = TestStatus::WARNING;
                test.messages.push_back(
                    "FMI 3.0: diagram.svg exists in documentation/ but diagram.png is missing (required "
                    "if diagram.svg is provided).");
            }
        }
        cert.printTestResult(test);
    }

    // 3. Terminals and Icons Files
    {
        TestResult test{"Terminals and Icons Files", TestStatus::PASS, {}};
        if (std::filesystem::exists(path / "terminalsAndIcons"))
        {
            if (std::filesystem::exists(path / "terminalsAndIcons" / "icon.svg") &&
                !std::filesystem::exists(path / "terminalsAndIcons" / "icon.png"))
            {
                test.status = TestStatus::WARNING;
                test.messages.push_back(
                    "FMI 3.0: icon.svg exists in terminalsAndIcons/ but icon.png is missing (required "
                    "if icon.svg is provided).");
            }
        }
        cert.printTestResult(test);
    }

    // 4. Binaries and Sources
    {
        TestResult test{"Binaries and Sources", TestStatus::PASS, {}};
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
    }
}
