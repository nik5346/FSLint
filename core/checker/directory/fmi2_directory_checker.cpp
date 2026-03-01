#include "fmi2_directory_checker.h"

#include "certificate.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <map>
#include <set>
#include <string>

void Fmi2DirectoryChecker::performVersionSpecificChecks(const std::filesystem::path& path, Certificate& cert,
                                                        const std::map<std::string, std::string>& model_identifiers,
                                                        const std::set<std::string>& listed_sources_in_md,
                                                        [[maybe_unused]] bool needs_execution_tool)
{
    // 1. FMU Root Entries
    {
        TestResult test{"FMU Root Entries", TestStatus::PASS, {}};

        static const std::set<std::string> fmi2_standard_entries = {"modelDescription.xml",
                                                                    "model.png",
                                                                    "documentation",
                                                                    "licenses",
                                                                    "sources",
                                                                    "binaries",
                                                                    "resources",
                                                                    "extra",
                                                                    "terminalsAndIcons",
                                                                    "buildDescription.xml"};

        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            const std::string name = entry.path().filename().string();
            if (!fmi2_standard_entries.contains(name))
            {
                test.status = TestStatus::WARNING;
                const std::string type = entry.is_directory() ? "directory" : "file";
                test.messages.push_back(std::format("Unknown {} in FMU root: '{}'.", type, name));
            }

            if (entry.is_directory() && fmi2_standard_entries.contains(name) && isEffectivelyEmpty(entry.path()))
            {
                test.status = TestStatus::WARNING;
                test.messages.push_back("Standard directory '" + name + "' is empty.");
            }
        }
        cert.printTestResult(test);
    }

    // 2. model.png Existence
    {
        TestResult test{"model.png Existence", TestStatus::PASS, {}};
        if (!std::filesystem::exists(path / "model.png"))
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Recommended file 'model.png' is missing from the FMU root.");
        }
        cert.printTestResult(test);
    }

    // 3. Documentation and Licenses
    {
        TestResult test{"Documentation and Licenses", TestStatus::PASS, {}};
        auto doc_path = path / "documentation";

        if (needs_execution_tool)
        {
            if (!std::filesystem::exists(doc_path / "externalDependencies.txt") &&
                !std::filesystem::exists(doc_path / "externalDependencies.html"))
            {
                if (test.status != TestStatus::FAIL)
                    test.status = TestStatus::WARNING;
                test.messages.push_back("needsExecutionTool is true, but "
                                        "'documentation/externalDependencies.{txt|html}' is missing.");
            }
        }

        auto licenses_path = path / "licenses";
        if (std::filesystem::exists(licenses_path))
        {
            if (!std::filesystem::exists(licenses_path / "license.txt") &&
                !std::filesystem::exists(licenses_path / "license.html"))
            {
                if (test.status != TestStatus::FAIL)
                    test.status = TestStatus::WARNING;
                test.messages.push_back("'licenses/' exists but does not contain "
                                        "a 'license.txt' or 'license.html' entry point.");
            }
        }
        cert.printTestResult(test);
    }

    // 4. Distribution (Binaries and Sources)
    bool has_binaries = false;
    bool has_build_description = false;
    auto sources_path = path / "sources";
    {
        TestResult test{"Binaries and Sources", TestStatus::PASS, {}};

        if (std::filesystem::exists(path / "binaries"))
        {
            for (const auto& entry : std::filesystem::directory_iterator(path / "binaries"))
            {
                if (entry.is_directory())
                {
                    for (const auto& [interface, model_id] : model_identifiers)
                    {
                        for (const auto& ext : {".dll", ".so", ".dylib", ".lib", ".a"})
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

        has_build_description = std::filesystem::exists(sources_path / "buildDescription.xml") ||
                                std::filesystem::exists(path / "buildDescription.xml");
        const bool has_sources = !listed_sources_in_md.empty() || has_build_description ||
                                 (std::filesystem::exists(sources_path) && !isEffectivelyEmpty(sources_path));

        if (!has_binaries && !has_sources)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back(
                "FMU must contain either a precompiled binary for at least one platform or source code.");
        }
        cert.printTestResult(test);
    }

    // 5. Source Files Consistency
    {
        TestResult test{"Source Files Consistency", TestStatus::PASS, {}};
        if (std::filesystem::exists(sources_path) && !has_build_description)
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(sources_path))
            {
                if (entry.is_regular_file())
                {
                    auto rel_path = std::filesystem::relative(entry.path(), sources_path);
                    std::string filename = rel_path.string();
                    std::replace(filename.begin(), filename.end(), '\\', '/'); // Normalize paths

                    // Only check typical source files
                    static const std::set<std::string> source_extensions = {".c", ".cc", ".cpp", ".cxx", ".C", ".c++"};
                    const std::string ext = entry.path().extension().string();

                    if (source_extensions.contains(ext))
                    {
                        if (!listed_sources_in_md.contains(filename))
                        {
                            test.status = TestStatus::WARNING;
                            test.messages.push_back(
                                std::format("Source file '{}' exists in 'sources/' directory but is not listed in "
                                            "'modelDescription.xml'.",
                                            filename));
                        }
                    }
                }
            }
        }
        cert.printTestResult(test);
    }

    // 6. FMI 2.0.4 Compatibility
    {
        const bool has_build_description_anywhere = has_build_description;
        const bool has_sources = !listed_sources_in_md.empty() || has_build_description_anywhere ||
                                 (std::filesystem::exists(sources_path) && !isEffectivelyEmpty(sources_path));

        if (has_sources)
        {
            TestResult test{"FMI 2.0.4 Compatibility", TestStatus::PASS, {}};
            const bool has_sources_in_md = !listed_sources_in_md.empty();
            if (has_sources_in_md && !has_build_description_anywhere)
            {
                test.status = TestStatus::WARNING;
                test.messages.push_back("Source code FMU only contains <SourceFiles> in modelDescription.xml. "
                                        "It is recommended to also provide a buildDescription.xml for FMI 2.0.4+ "
                                        "compatibility.");
            }
            else if (!has_sources_in_md && has_build_description_anywhere)
            {
                test.status = TestStatus::WARNING;
                test.messages.push_back("Source code FMU only contains buildDescription.xml. For backwards "
                                        "compatibility with older FMI 2.0 importers, it is recommended to also provide "
                                        "<SourceFiles> in modelDescription.xml.");
            }
            cert.printTestResult(test);
        }
    }

    // 7. Standard Headers
    static const std::set<std::string> fmi2_headers = {"fmi2Functions.h", "fmi2FunctionTypes.h", "fmi2TypesPlatform.h"};
    checkStandardHeaders(path, cert, fmi2_headers);
}
