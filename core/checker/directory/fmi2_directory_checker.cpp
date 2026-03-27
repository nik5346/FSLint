#include "fmi2_directory_checker.h"

#include "certificate.h"
#include "file_utils.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <map>
#include <set>
#include <string>

void Fmi2DirectoryChecker::performVersionSpecificChecks(const std::filesystem::path& path, Certificate& cert,
                                                        const std::map<std::string, std::string>& model_identifiers,
                                                        const std::set<std::string>& listed_sources_in_md,
                                                        [[maybe_unused]] bool needs_execution_tool) const
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
            const std::string name = file_utils::pathToUtf8(entry.path().filename());
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
        auto png_path = path / "model.png";
        if (!std::filesystem::exists(png_path))
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Recommended file 'model.png' is missing from the FMU root.");
        }
        else
        {
            auto dimensions = file_utils::getPngDimensions(png_path);
            if (dimensions)
            {
                if (dimensions->first < 100 || dimensions->second < 100)
                {
                    test.status = TestStatus::WARNING;
                    test.messages.push_back(
                        std::format("Icon 'model.png' is small ({}x{} pixels). A size of at least 100x100 pixels is "
                                    "recommended.",
                                    dimensions->first, dimensions->second));
                }
            }
        }
        cert.printTestResult(test);
    }

    // 3. Documentation and Licenses
    {
        TestResult test{"Documentation and Licenses", TestStatus::PASS, {}};
        auto doc_path = path / "documentation";

        auto licenses_sub_path = doc_path / "licenses";
        if (std::filesystem::exists(licenses_sub_path) && std::filesystem::is_directory(licenses_sub_path) &&
            isEffectivelyEmpty(licenses_sub_path))
        {
            const TestResult empty_test{
                "Empty Subdirectory", TestStatus::WARNING, {"Standard directory 'documentation/licenses' is empty."}};
            cert.printTestResult(empty_test);
        }

        if (std::filesystem::exists(doc_path))
        {
            if (!std::filesystem::exists(doc_path / "index.html"))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("The documentation entry point 'documentation/index.html' is missing.");
            }
        }
        else
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Providing documentation is recommended.");
        }

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
                test.status = TestStatus::FAIL;
                test.messages.push_back("The license entry point (e.g. 'licenses/license.txt') is missing.");
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
            std::set<std::string> unique_model_ids;
            for (const auto& [interface, model_id] : model_identifiers)
                unique_model_ids.insert(model_id);

            for (const auto& entry : std::filesystem::directory_iterator(path / "binaries"))
            {
                if (entry.is_directory())
                {
                    const std::string platform = file_utils::pathToUtf8(entry.path().filename());
                    for (const auto& model_id : unique_model_ids)
                    {
                        bool found_model_id = false;
                        for (const auto& ext : {".dll", ".so", ".dylib", ".lib", ".a"})
                        {
                            if (std::filesystem::exists(entry.path() / (model_id + ext)))
                            {
                                found_model_id = true;
                                has_binaries = true;
                                break;
                            }
                        }

                        if (!found_model_id)
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back(
                                std::format("Platform directory '{}' does not contain a binary matching "
                                            "modelIdentifier '{}'.",
                                            platform, model_id));
                        }
                    }
                }
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
        const bool has_physical_sources = std::filesystem::exists(sources_path) && !isEffectivelyEmpty(sources_path);
        const bool has_sources_in_md = !listed_sources_in_md.empty();

        if (has_physical_sources && !has_sources_in_md)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Source code FMU contains a 'sources/' directory, but no <SourceFiles> are listed "
                                    "in 'modelDescription.xml'.");
        }
        else if (has_physical_sources && !has_build_description)
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(sources_path))
            {
                if (entry.is_regular_file())
                {
                    auto rel_path = std::filesystem::relative(entry.path(), sources_path);
                    std::string filename = file_utils::pathToUtf8(rel_path);
                    std::replace(filename.begin(), filename.end(), '\\', '/'); // Normalize paths

                    // Only check typical source files
                    static const std::set<std::string> source_extensions = {".c", ".cc", ".cpp", ".cxx", ".C", ".c++"};
                    const std::string ext = file_utils::pathToUtf8(entry.path().extension());

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

    // 6. 2.0.4 Compatibility
    {
        const bool has_build_description_anywhere = has_build_description;
        const bool has_physical_sources = std::filesystem::exists(sources_path) && !isEffectivelyEmpty(sources_path);
        const bool has_sources_in_md = !listed_sources_in_md.empty();

        if (has_physical_sources || has_sources_in_md || has_build_description_anywhere)
        {
            TestResult test{"2.0.4 Compatibility", TestStatus::PASS, {}};
            if ((has_physical_sources || has_sources_in_md) && !has_build_description_anywhere)
            {
                test.status = TestStatus::WARNING;
                test.messages.push_back("Providing a 'buildDescription.xml' is recommended for source code FMUs (FMI "
                                        "2.0.4+).");
            }
            else if (!has_sources_in_md && has_build_description_anywhere)
            {
                test.status = TestStatus::WARNING;
                test.messages.push_back("Source code FMU only contains buildDescription.xml. For backwards "
                                        "compatibility with older importers, it is recommended to also provide "
                                        "<SourceFiles> in modelDescription.xml.");
            }
            cert.printTestResult(test);
        }
    }

    // 7. Standard Headers
    static const std::set<std::string> fmi2_headers = {"fmi2Functions.h", "fmi2FunctionTypes.h", "fmi2TypesPlatform.h"};
    checkStandardHeaders(path, cert, fmi2_headers);
}
