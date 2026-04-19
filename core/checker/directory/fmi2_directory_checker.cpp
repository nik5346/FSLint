#include "fmi2_directory_checker.h"

#include "certificate.h"
#include "file_utils.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <map>
#include <regex>
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

        static const std::set<std::string> fmi2_standard_entries = {
            "modelDescription.xml", "model.png",           "documentation", "sources", "binaries", "resources", "extra",
            "terminalsAndIcons",    "buildDescription.xml"};

        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            const std::string name = file_utils::pathToUtf8(entry.path().filename());
            if (!fmi2_standard_entries.contains(name))
            {
                test.setStatus(TestStatus::WARNING);
                const std::string type = entry.is_directory() ? "directory" : "file";
                test.getMessages().emplace_back(std::format("Unknown {} in FMU root: '{}'.", type, name));
            }

            if (entry.is_directory() && fmi2_standard_entries.contains(name) && isEffectivelyEmpty(entry.path()))
            {
                test.setStatus(TestStatus::WARNING);
                test.getMessages().emplace_back(std::format("Standard directory '{}' is empty.", name));
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
            test.setStatus(TestStatus::WARNING);
            test.getMessages().emplace_back("Recommended file 'model.png' is missing from the FMU root.");
        }
        else
        {
            auto dimensions = file_utils::getPngDimensions(png_path);
            if (dimensions)
            {
                if (dimensions->first < 100 || dimensions->second < 100)
                {
                    test.setStatus(TestStatus::WARNING);
                    test.getMessages().emplace_back(
                        std::format("Icon 'model.png' is small ({}x{} pixels). A size of at least 100x100 pixels is "
                                    "recommended.",
                                    dimensions->first, dimensions->second));
                }
            }
        }
        cert.printTestResult(test);
    }

    // 3. Documentation
    {
        TestResult test{"Documentation", TestStatus::PASS, {}};
        auto doc_path = path / "documentation";

        if (std::filesystem::exists(doc_path))
        {
            if (!std::filesystem::exists(doc_path / "index.html"))
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back("The documentation entry point 'documentation/index.html' is missing.");
            }
        }
        else
        {
            test.setStatus(TestStatus::WARNING);
            test.getMessages().emplace_back("Providing documentation is recommended.");
        }

        if (needs_execution_tool)
        {
            if (!std::filesystem::exists(doc_path / "externalDependencies.txt") &&
                !std::filesystem::exists(doc_path / "externalDependencies.html"))
            {
                if (test.getStatus() != TestStatus::FAIL)
                    test.setStatus(TestStatus::WARNING);
                test.getMessages().emplace_back(
                    "Since needsExecutionTool='true', 'documentation/externalDependencies.{txt|html}' should be "
                    "present to document the external resources the FMU depends on.");
            }
        }
        cert.printTestResult(test);
    }

    // 4. Licenses
    {
        TestResult test{"Licenses", TestStatus::PASS, {}};
        auto licenses_path = path / "documentation" / "licenses";

        if (std::filesystem::exists(licenses_path))
        {
            if (!std::filesystem::exists(licenses_path / "license.txt") &&
                !std::filesystem::exists(licenses_path / "license.html"))
            {
                test.setStatus(TestStatus::FAIL);
                if (std::filesystem::is_directory(licenses_path) && isEffectivelyEmpty(licenses_path))
                {
                    test.getMessages().emplace_back("Standard directory 'documentation/licenses' is empty.");
                }
                else
                {
                    test.getMessages().emplace_back(
                        "The license entry point 'documentation/licenses/license.txt' is missing.");
                }
            }
        }
        else
        {
            test.setStatus(TestStatus::WARNING);
            test.getMessages().emplace_back("Providing a license is recommended in 'documentation/licenses/'.");
        }
        cert.printTestResult(test);
    }

    // 5. Distribution (Binaries and Sources)
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

                    static const std::set<std::string> fmi2_platforms = {"win32",   "win64",    "linux32",
                                                                         "linux64", "darwin32", "darwin64"};
                    if (!fmi2_platforms.contains(platform))
                    {
                        if (test.getStatus() != TestStatus::FAIL)
                            test.setStatus(TestStatus::WARNING);
                        test.getMessages().emplace_back(
                            std::format("Platform directory '{}' is not one of the standardized FMI 2.0 platform "
                                        "names (win32, win64, linux32, linux64, darwin32, darwin64).",
                                        platform));
                    }

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
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(
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
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                "FMU must contain either a precompiled binary for at least one platform or source code.");
        }
        cert.printTestResult(test);
    }

    // 6. Source Files Consistency
    {
        TestResult test{"Source Files Consistency", TestStatus::PASS, {}};
        const bool has_physical_sources = std::filesystem::exists(sources_path) && !isEffectivelyEmpty(sources_path);
        const bool has_sources_in_md = !listed_sources_in_md.empty();

        if (has_physical_sources && !has_sources_in_md)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                "Source code FMU contains a 'sources/' directory, but no <SourceFiles> are listed "
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
                    std::ranges::replace(filename, '\\', '/'); // Normalize paths

                    // Only check typical source files
                    static const std::set<std::string> source_extensions = {".c", ".cc", ".cpp", ".cxx", ".C", ".c++"};
                    const std::string ext = file_utils::pathToUtf8(entry.path().extension());

                    if (source_extensions.contains(ext))
                    {
                        if (!listed_sources_in_md.contains(filename))
                        {
                            test.setStatus(TestStatus::WARNING);
                            test.getMessages().emplace_back(
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

    // 7. 2.0.4 Compatibility
    {
        const bool has_build_description_anywhere = has_build_description;
        const bool has_physical_sources = std::filesystem::exists(sources_path) && !isEffectivelyEmpty(sources_path);
        const bool has_sources_in_md = !listed_sources_in_md.empty();

        if (has_physical_sources || has_sources_in_md || has_build_description_anywhere)
        {
            TestResult test{"2.0.4 Compatibility", TestStatus::PASS, {}};
            if ((has_physical_sources || has_sources_in_md) && !has_build_description_anywhere)
            {
                test.setStatus(TestStatus::WARNING);
                test.getMessages().emplace_back(
                    "Providing a 'buildDescription.xml' is recommended for source code FMUs (FMI "
                    "2.0.4+).");
            }
            else if (!has_sources_in_md && has_build_description_anywhere)
            {
                test.setStatus(TestStatus::WARNING);
                test.getMessages().emplace_back("Source code FMU only contains buildDescription.xml. For backwards "
                                                "compatibility with older importers, it is recommended to also provide "
                                                "<SourceFiles> in modelDescription.xml.");
            }
            cert.printTestResult(test);
        }
    }

    // 8. Extra
    {
        TestResult test{"Extra", TestStatus::PASS, {}};
        const auto extra_path = path / "extra";
        if (std::filesystem::exists(extra_path))
        {
            for (const auto& entry : std::filesystem::directory_iterator(extra_path))
            {
                if (entry.is_directory())
                {
                    const std::string name = file_utils::pathToUtf8(entry.path().filename());
                    // Reverse domain notation: e.g. com.example
                    // It should have at least one dot and follow basic domain naming rules.
                    // We allow hyphens and uppercase letters as they are common in RDNN.
                    const std::regex rd_regex("^[a-zA-Z0-9-]+(\\.[a-zA-Z0-9-]+)+$");
                    if (!std::regex_match(name, rd_regex))
                    {
                        if (test.getStatus() != TestStatus::FAIL)
                            test.setStatus(TestStatus::WARNING);
                        test.getMessages().emplace_back(
                            std::format("Subdirectory '{}' in extra/ should use reverse domain name notation "
                                        "(e.g. 'com.example').",
                                        name));
                    }
                }
            }
        }
        cert.printTestResult(test);
    }

    // 9. Standard Headers
    static const std::set<std::string> fmi2_headers = {"fmi2Functions.h", "fmi2FunctionTypes.h", "fmi2TypesPlatform.h"};
    checkStandardHeaders(path, cert, fmi2_headers);
}
