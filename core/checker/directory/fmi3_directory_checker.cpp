#include "fmi3_directory_checker.h"

#include "certificate.h"

#include <filesystem>
#include <format>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <string_view>

void Fmi3DirectoryChecker::performVersionSpecificChecks(
    const std::filesystem::path& path, Certificate& cert, const std::map<std::string, std::string>& model_identifiers,
    [[maybe_unused]] const std::set<std::string>& listed_sources_in_md, bool needs_execution_tool)
{
    // 1. FMU Root Entries
    {
        TestResult test{"FMU Root Entries", TestStatus::PASS, {}};

        static const std::set<std::string> fmi3_standard_entries = {
            "modelDescription.xml", "documentation", "terminalsAndIcons", "sources", "binaries", "resources", "extra"};

        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            const std::string name = entry.path().filename().string();
            if (!fmi3_standard_entries.contains(name))
            {
                test.status = TestStatus::WARNING;
                const std::string type = entry.is_directory() ? "directory" : "file";
                test.messages.push_back(std::format("Unknown {} in FMU root: '{}'.", type, name));
            }

            if (entry.is_directory() && fmi3_standard_entries.contains(name) && isEffectivelyEmpty(entry.path()))
            {
                test.status = TestStatus::WARNING;
                test.messages.push_back("Standard directory '" + name + "' is empty.");
            }
        }
        cert.printTestResult(test);
    }

    // 2. Documentation Files
    {
        TestResult test{"Documentation Files", TestStatus::PASS, {}};
        auto doc_path = path / "documentation";

        // index.html check (recommended entry point)
        if (!std::filesystem::exists(doc_path / "index.html"))
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Recommended entry point 'documentation/index.html' is missing.");
        }

        // externalDependencies check (must be present even if documentation/ is missing)
        if (needs_execution_tool)
        {
            if (!std::filesystem::exists(doc_path / "externalDependencies.txt") &&
                !std::filesystem::exists(doc_path / "externalDependencies.html"))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("needsExecutionTool is true, but "
                                        "'documentation/externalDependencies.{txt|html}' is missing.");
            }
        }

        if (std::filesystem::exists(doc_path))
        {
            // diagram.svg/png check
            if (std::filesystem::exists(doc_path / "diagram.svg") && !std::filesystem::exists(doc_path / "diagram.png"))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("diagram.svg exists in documentation/ but diagram.png is missing (required "
                                        "if diagram.svg is provided).");
            }

            // licenses directory check
            for (const auto& entry_name : {"license", "licenses"})
            {
                auto licenses_path = doc_path / entry_name;
                if (std::filesystem::exists(licenses_path))
                {
                    if (std::filesystem::is_directory(licenses_path) && isEffectivelyEmpty(licenses_path))
                    {
                        TestResult empty_test{
                            "Empty Subdirectory",
                            TestStatus::WARNING,
                            {"Standard directory 'documentation/" + std::string(entry_name) + "' is empty."}};
                        cert.printTestResult(empty_test);
                    }

                    if (!std::filesystem::exists(licenses_path / "license.spdx") &&
                        !std::filesystem::exists(licenses_path / "license.txt") &&
                        !std::filesystem::exists(licenses_path / "license.html"))
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("'documentation/" + std::string(entry_name) +
                                                "/' exists but does not contain "
                                                "a 'license.spdx', 'license.txt', or 'license.html' entry point.");
                    }
                }
            }
        }
        cert.printTestResult(test);
    }

    // 3. Terminals and Icons Files
    {
        TestResult test{"Terminals and Icons Files", TestStatus::PASS, {}};
        auto tai_path = path / "terminalsAndIcons";
        const bool icon_png_missing = !std::filesystem::exists(tai_path / "icon.png");
        const bool icon_svg_exists = std::filesystem::exists(tai_path / "icon.svg");

        if (std::filesystem::exists(tai_path))
        {
            for (const auto& entry : std::filesystem::directory_iterator(tai_path))
            {
                if (entry.path().extension() == ".svg")
                {
                    auto png_path = entry.path();
                    png_path.replace_extension(".png");
                    if (!std::filesystem::exists(png_path))
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back(
                            std::format("'{}' exists in terminalsAndIcons/ but '{}' is missing (required as "
                                        "fallback).",
                                        entry.path().filename().string(), png_path.filename().string()));
                    }
                }
            }
        }

        if (icon_png_missing && !icon_svg_exists)
        {
            if (test.status != TestStatus::FAIL)
                test.status = TestStatus::WARNING;
            test.messages.push_back("Recommended file 'terminalsAndIcons/icon.png' is missing.");
        }
        cert.printTestResult(test);
    }

    // 4. Sources
    {
        TestResult test{"Sources", TestStatus::PASS, {}};
        auto sources_path = path / "sources";
        if (std::filesystem::exists(sources_path))
        {
            if (!std::filesystem::exists(sources_path / "buildDescription.xml"))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("'sources/' directory exists but 'sources/buildDescription.xml' is missing.");
            }
        }
        cert.printTestResult(test);
    }

    // 5. Binaries
    {
        TestResult test{"Binaries", TestStatus::PASS, {}};
        auto binaries_path = path / "binaries";
        bool has_binaries = false;
        bool static_library_detected = false;

        if (std::filesystem::exists(binaries_path))
        {
            for (const auto& entry : std::filesystem::directory_iterator(binaries_path))
            {
                if (entry.is_directory())
                {
                    const std::string tuple = entry.path().filename().string();
                    const std::regex tuple_regex("^([a-z0-9_]+)-([a-z0-9_]+)(-([a-z0-9_]+))?$");
                    std::smatch match;
                    if (!std::regex_match(tuple, match, tuple_regex))
                    {
                        if (test.status != TestStatus::FAIL)
                            test.status = TestStatus::WARNING;
                        test.messages.push_back(
                            std::format("Platform tuple '{}' does not follow the <arch>-<sys>[-<abi>] format.", tuple));
                    }
                    else if (match[4].matched) // ABI present
                    {
                        static_library_detected = true;
                        const std::string abi = match[4].str();
                        const std::regex abi_regex("^[a-z][a-z0-9_]*$");
                        if (!std::regex_match(abi, abi_regex))
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back(std::format(
                                "ABI name '{}' in platform tuple '{}' is invalid (must start with lowercase "
                                "letter and contain only lowercase letters, digits, or underscores).",
                                abi, tuple));
                        }
                    }

                    // Check for modelIdentifier.<ext>
                    bool found_model_id = false;
                    for (const auto& [interface, model_id] : model_identifiers)
                    {
                        for (const std::string_view ext : {".dll", ".so", ".dylib", ".lib", ".a"})
                        {
                            if (std::filesystem::exists(entry.path() / (model_id + std::string(ext))))
                            {
                                found_model_id = true;
                                has_binaries = true;
                                if (ext == ".lib" || ext == ".a")
                                    static_library_detected = true;
                                break;
                            }
                        }
                        if (found_model_id)
                            break;
                    }
                    if (!found_model_id && !model_identifiers.empty())
                    {
                        if (test.status != TestStatus::FAIL)
                            test.status = TestStatus::WARNING;
                        test.messages.push_back(std::format(
                            "Platform directory '{}' does not contain a binary matching any modelIdentifier.", tuple));
                    }
                }
            }
        }

        if (static_library_detected)
        {
            if (!std::filesystem::exists(path / "documentation" / "staticLinking.txt") &&
                !std::filesystem::exists(path / "documentation" / "staticLinking.html"))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back(
                    "Static library detected, but 'documentation/staticLinking.{txt|html}' is missing.");
            }
        }
        cert.printTestResult(test);

        // 6. Implementation Presence
        TestResult impl_test{"Implementation Presence", TestStatus::PASS, {}};
        const bool has_sources = std::filesystem::exists(path / "sources" / "buildDescription.xml");
        if (!has_binaries && !has_sources)
        {
            impl_test.status = TestStatus::FAIL;
            impl_test.messages.push_back("FMU must contain at least one implementation (binary or source code "
                                         "with buildDescription.xml).");
        }
        cert.printTestResult(impl_test);
    }

    // 7. Extra
    {
        TestResult test{"Extra", TestStatus::PASS, {}};
        auto extra_path = path / "extra";
        if (std::filesystem::exists(extra_path))
        {
            for (const auto& entry : std::filesystem::directory_iterator(extra_path))
            {
                if (entry.is_directory())
                {
                    const std::string name = entry.path().filename().string();
                    // Reverse domain notation: e.g. com.example
                    // It should have at least one dot and follow basic domain naming rules.
                    const std::regex rd_regex("^[a-z0-9]+(\\.[a-z0-9]+)+$");
                    if (!std::regex_match(name, rd_regex))
                    {
                        if (test.status != TestStatus::FAIL)
                            test.status = TestStatus::WARNING;
                        test.messages.push_back(
                            std::format("Subdirectory '{}' in extra/ should use reverse domain name notation "
                                        "(e.g. 'com.example').",
                                        name));
                    }
                }
            }
        }
        cert.printTestResult(test);
    }

    // 8. Standard Headers
    static const std::set<std::string> fmi3_headers = {"fmi3Functions.h", "fmi3FunctionTypes.h", "fmi3PlatformTypes.h"};
    checkStandardHeaders(path, cert, fmi3_headers);
}
