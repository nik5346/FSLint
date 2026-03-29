#include "fmi3_directory_checker.h"

#include "certificate.h"
#include "file_utils.h"

#include <filesystem>
#include <format>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <string_view>

void Fmi3DirectoryChecker::performVersionSpecificChecks(
    const std::filesystem::path& path, Certificate& cert, const std::map<std::string, std::string>& model_identifiers,
    [[maybe_unused]] const std::set<std::string>& listed_sources_in_md, bool needs_execution_tool) const
{
    // 1. FMU Root Entries
    {
        TestResult test{"FMU Root Entries", TestStatus::PASS, {}};

        static const std::set<std::string> fmi3_standard_entries = {
            "modelDescription.xml", "documentation", "terminalsAndIcons", "sources", "binaries", "resources", "extra"};

        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            const std::string name = file_utils::pathToUtf8(entry.path().filename());
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

    // 2. Documentation
    {
        TestResult test{"Documentation", TestStatus::PASS, {}};
        auto doc_path = path / "documentation";

        // index.html check (recommended entry point)
        if (std::filesystem::exists(doc_path))
        {
            if (isEffectivelyEmpty(doc_path))
            {
                test.status = TestStatus::WARNING;
                test.messages.push_back("Standard directory 'documentation' is empty.");
            }

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
            auto diag_png_path = doc_path / "diagram.png";
            if (std::filesystem::exists(doc_path / "diagram.svg") && !std::filesystem::exists(diag_png_path))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("diagram.svg exists in documentation/ but diagram.png is missing (required "
                                        "if diagram.svg is provided).");
            }

            if (std::filesystem::exists(diag_png_path))
            {
                auto dimensions = file_utils::getPngDimensions(diag_png_path);
                if (dimensions)
                {
                    if (dimensions->first < 100 || dimensions->second < 100)
                    {
                        if (test.status != TestStatus::FAIL)
                            test.status = TestStatus::WARNING;
                        test.messages.push_back(std::format(
                            "Diagram 'documentation/diagram.png' is small ({}x{} pixels). A size of at least "
                            "100x100 pixels is recommended.",
                            dimensions->first, dimensions->second));
                    }
                }
            }
        }
        cert.printTestResult(test);
    }

    // 3. Licenses
    {
        TestResult test{"Licenses", TestStatus::PASS, {}};
        auto licenses_path = path / "documentation" / "licenses";

        if (std::filesystem::exists(licenses_path))
        {
            if (!std::filesystem::exists(licenses_path / "license.spdx") &&
                !std::filesystem::exists(licenses_path / "license.txt") &&
                !std::filesystem::exists(licenses_path / "license.html"))
            {
                test.status = TestStatus::FAIL;
                if (std::filesystem::is_directory(licenses_path) && isEffectivelyEmpty(licenses_path))
                {
                    test.messages.push_back("Standard directory 'documentation/licenses' is empty.");
                }
                else
                {
                    test.messages.push_back(
                        "The license entry point (e.g. 'documentation/licenses/license.txt') is missing.");
                }
            }
        }
        else
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Providing a license is recommended (e.g. in 'documentation/licenses/').");
        }
        cert.printTestResult(test);
    }

    // 3. Terminals and Icons Files
    {
        TestResult test{"Terminals and Icons Files", TestStatus::PASS, {}};
        auto tai_path = path / "terminalsAndIcons";
        auto icon_png_path = tai_path / "icon.png";
        const bool icon_png_exists = std::filesystem::exists(icon_png_path);
        const bool icon_svg_exists = std::filesystem::exists(tai_path / "icon.svg");

        if (std::filesystem::exists(tai_path))
        {
            for (const auto& entry : std::filesystem::directory_iterator(tai_path))
            {
                if (entry.path().extension() == ".svg" && entry.path().stem() != "icon")
                {
                    auto png_path = entry.path();
                    png_path.replace_extension(".png");
                    if (!std::filesystem::exists(png_path))
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back(
                            std::format("'{}' exists in terminalsAndIcons/ but '{}' is missing (required as "
                                        "fallback).",
                                        file_utils::pathToUtf8(entry.path().filename()),
                                        file_utils::pathToUtf8(png_path.filename())));
                    }
                    else
                    {
                        auto dimensions = file_utils::getPngDimensions(png_path);
                        if (dimensions)
                        {
                            if (dimensions->first < 100 || dimensions->second < 100)
                            {
                                if (test.status != TestStatus::FAIL)
                                    test.status = TestStatus::WARNING;
                                test.messages.push_back(std::format(
                                    "Icon '{}' is small ({}x{} pixels). A size of at least 100x100 pixels is "
                                    "recommended.",
                                    file_utils::pathToUtf8(png_path.filename()), dimensions->first,
                                    dimensions->second));
                            }
                        }
                    }
                }
            }
        }

        if (icon_png_exists)
        {
            auto dimensions = file_utils::getPngDimensions(icon_png_path);
            if (dimensions)
            {
                if (dimensions->first < 100 || dimensions->second < 100)
                {
                    if (test.status != TestStatus::FAIL)
                        test.status = TestStatus::WARNING;
                    test.messages.push_back(
                        std::format("Icon 'terminalsAndIcons/icon.png' is small ({}x{} pixels). A size of at least "
                                    "100x100 pixels is recommended.",
                                    dimensions->first, dimensions->second));
                }
            }
        }

        if (!icon_png_exists && !icon_svg_exists)
        {
            if (test.status != TestStatus::FAIL)
                test.status = TestStatus::WARNING;
            test.messages.push_back("Recommended file 'terminalsAndIcons/icon.png' is missing.");
        }
        else if (icon_svg_exists && !icon_png_exists)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back(
                "terminalsAndIcons/icon.svg exists but icon.png is missing (required as fallback).");
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
            std::set<std::string> unique_model_ids;
            for (const auto& [interface, model_id] : model_identifiers)
                unique_model_ids.insert(model_id);

            for (const auto& entry : std::filesystem::directory_iterator(binaries_path))
            {
                if (entry.is_directory())
                {
                    const std::string tuple = file_utils::pathToUtf8(entry.path().filename());
                    const std::regex tuple_regex("^([a-z0-9_]+)-([a-z0-9_]+)(-([a-z0-9_]+))?$");
                    std::smatch match;
                    if (!std::regex_match(tuple, match, tuple_regex))
                    {
                        if (test.status != TestStatus::FAIL)
                            test.status = TestStatus::WARNING;
                        test.messages.push_back(
                            std::format("Platform tuple '{}' does not follow the <arch>-<sys>[-<abi>] format.", tuple));
                    }
                    else
                    {
                        const std::string arch = match[1].str();
                        const std::string sys = match[2].str();

                        static const std::set<std::string> fmi3_architectures = {
                            "aarch32", "aarch64", "riscv32", "riscv64", "x86", "x86_64", "ppc32", "ppc64"};
                        if (!fmi3_architectures.contains(arch))
                        {
                            if (test.status != TestStatus::FAIL)
                                test.status = TestStatus::WARNING;
                            test.messages.push_back(std::format(
                                "Architecture '{}' in platform tuple '{}' is not one of the standardized FMI 3.0 "
                                "architectures (aarch32, aarch64, riscv32, riscv64, x86, x86_64, ppc32, ppc64).",
                                arch, tuple));
                        }

                        static const std::set<std::string> fmi3_systems = {"darwin", "linux", "windows"};
                        if (!fmi3_systems.contains(sys))
                        {
                            if (test.status != TestStatus::FAIL)
                                test.status = TestStatus::WARNING;
                            test.messages.push_back(std::format(
                                "Operating system '{}' in platform tuple '{}' is not one of the standardized "
                                "FMI 3.0 systems (darwin, linux, windows).",
                                sys, tuple));
                        }
                    }

                    if (match[4].matched) // ABI present
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
                    for (const auto& model_id : unique_model_ids)
                    {
                        bool found_model_id = false;
                        for (const std::string_view ext : {".dll", ".so", ".dylib", ".lib", ".a"})
                        {
                            // Check direct binary (e.g. binaries/x64-windows/model.dll)
                            // or subdirectory (e.g. binaries/x64-windows/model/model.dll)
                            if (std::filesystem::exists(entry.path() / (model_id + std::string(ext))) ||
                                std::filesystem::exists(entry.path() / model_id / (model_id + std::string(ext))))
                            {
                                found_model_id = true;
                                has_binaries = true;
                                if (ext == ".lib" || ext == ".a")
                                    static_library_detected = true;
                                break;
                            }
                        }

                        if (!found_model_id)
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back(
                                std::format("Platform directory '{}' does not contain a binary matching "
                                            "modelIdentifier '{}'.",
                                            tuple, model_id));
                        }
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
                    const std::string name = file_utils::pathToUtf8(entry.path().filename());
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
