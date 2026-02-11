#include "fmi3_directory_checker.h"
#include "certificate.h"
#include <algorithm>
#include <regex>
#include <set>
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
            std::string name = entry.path().filename().string();
            if (!fmi3_standard_entries.contains(name))
            {
                test.status = TestStatus::WARNING;
                std::string type = entry.is_directory() ? "directory" : "file";
                test.messages.push_back("Unknown " + type + " in FMU root: '" + name + "'.");
            }

            if (entry.is_directory() && fmi3_standard_entries.contains(name) && std::filesystem::is_empty(entry.path()))
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

        // externalDependencies check (must be present even if documentation/ is missing)
        if (needs_execution_tool)
        {
            if (!std::filesystem::exists(doc_path / "externalDependencies.txt") &&
                !std::filesystem::exists(doc_path / "externalDependencies.html"))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("FMI 3.0: needsExecutionTool is true, but "
                                        "'documentation/externalDependencies.{txt|html}' is missing.");
            }
        }

        if (std::filesystem::exists(doc_path))
        {
            // index.html check
            if (!std::filesystem::exists(doc_path / "index.html"))
            {
                if (test.status != TestStatus::FAIL)
                    test.status = TestStatus::WARNING;
                test.messages.push_back("FMI 3.0: 'documentation/index.html' is missing (recommended entry point).");
            }

            // diagram.svg/png check
            if (std::filesystem::exists(doc_path / "diagram.svg") && !std::filesystem::exists(doc_path / "diagram.png"))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back(
                    "FMI 3.0: diagram.svg exists in documentation/ but diagram.png is missing (required "
                    "if diagram.svg is provided).");
            }

            // licenses directory check
            auto licenses_path = doc_path / "licenses";
            if (std::filesystem::exists(licenses_path))
            {
                if (!std::filesystem::exists(licenses_path / "license.spdx") &&
                    !std::filesystem::exists(licenses_path / "license.txt") &&
                    !std::filesystem::exists(licenses_path / "license.html"))
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("FMI 3.0: 'documentation/licenses/' exists but does not contain "
                                            "a 'license.spdx', 'license.txt', or 'license.html' entry point.");
                }
            }
        }
        cert.printTestResult(test);
    }

    // 3. Terminals and Icons Files
    {
        TestResult test{"Terminals and Icons Files", TestStatus::PASS, {}};
        auto tai_path = path / "terminalsAndIcons";
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
                        test.messages.push_back("FMI 3.0: '" + entry.path().filename().string() +
                                                "' exists in terminalsAndIcons/ but '" + png_path.filename().string() +
                                                "' is missing (required as fallback).");
                    }
                }
            }
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
                test.messages.push_back(
                    "FMI 3.0: 'sources/' directory exists but 'sources/buildDescription.xml' is missing.");
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
                    std::string tuple = entry.path().filename().string();
                    std::regex tuple_regex("^([a-z0-9_]+)-([a-z0-9_]+)(-([a-z0-9_]+))?$");
                    std::smatch match;
                    if (!std::regex_match(tuple, match, tuple_regex))
                    {
                        if (test.status != TestStatus::FAIL)
                            test.status = TestStatus::WARNING;
                        test.messages.push_back("FMI 3.0: Platform tuple '" + tuple +
                                                "' does not follow the <arch>-<sys>[-<abi>] format.");
                    }
                    else if (match[4].matched) // ABI present
                    {
                        static_library_detected = true;
                        std::string abi = match[4].str();
                        std::regex abi_regex("^[a-z][a-z0-9_]*$");
                        if (!std::regex_match(abi, abi_regex))
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back(
                                "FMI 3.0: ABI name '" + abi + "' in platform tuple '" + tuple +
                                "' is invalid (must start with lowercase letter and contain only lowercase letters, "
                                "digits, or underscores).");
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
                        test.messages.push_back("FMI 3.0: Platform directory '" + tuple +
                                                "' does not contain a binary matching any modelIdentifier.");
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
                    "FMI 3.0: Static library detected, but 'documentation/staticLinking.{txt|html}' is missing.");
            }
        }
        cert.printTestResult(test);

        // 6. Implementation Presence
        TestResult impl_test{"Implementation Presence", TestStatus::PASS, {}};
        bool has_sources = std::filesystem::exists(path / "sources" / "buildDescription.xml");
        if (!has_binaries && !has_sources)
        {
            impl_test.status = TestStatus::FAIL;
            impl_test.messages.push_back("FMI 3.0: FMU must contain at least one implementation (binary or source code "
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
                    std::string name = entry.path().filename().string();
                    // Reverse domain notation: e.g. com.example
                    // It should have at least one dot and follow basic domain naming rules.
                    std::regex rd_regex("^[a-z0-9]+(\\.[a-z0-9]+)+$");
                    if (!std::regex_match(name, rd_regex))
                    {
                        if (test.status != TestStatus::FAIL)
                            test.status = TestStatus::WARNING;
                        test.messages.push_back("FMI 3.0: Subdirectory '" + name +
                                                "' in extra/ should use reverse domain name notation (e.g. "
                                                "'com.example').");
                    }
                }
            }
        }
        cert.printTestResult(test);
    }
}
