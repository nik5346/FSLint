#include "fmi2_directory_checker.h"
#include "certificate.h"
#include <algorithm>
#include <set>

void Fmi2DirectoryChecker::performVersionSpecificChecks(const std::filesystem::path& path, Certificate& cert,
                                                        const std::map<std::string, std::string>& model_identifiers,
                                                        const std::set<std::string>& listed_sources_in_md)
{
    TestResult test{"FMU Structure", TestStatus::PASS, {}};

    static const std::set<std::string> fmi2_standard_entries = {"modelDescription.xml", "model.png", "documentation",
                                                                "licenses",             "sources",   "binaries",
                                                                "resources",            "extra",     "terminalsAndIcons",
                                                                "buildDescription.xml"};

    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        std::string name = entry.path().filename().string();
        if (!fmi2_standard_entries.contains(name))
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("Unknown entry in FMU root: '" + name + "'.");
        }
    }

    // Check for model.png in root
    if (!std::filesystem::exists(path / "model.png"))
    {
        if (test.status == TestStatus::PASS)
            test.status = TestStatus::WARNING;
        test.messages.push_back("FMI 2.0: Recommended file 'model.png' is missing from the FMU root.");
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

    auto sources_path = path / "sources";
    bool has_build_description =
        std::filesystem::exists(sources_path / "buildDescription.xml") || std::filesystem::exists(path / "buildDescription.xml");
    bool has_sources = !listed_sources_in_md.empty() || has_build_description ||
                       (std::filesystem::exists(sources_path) && !std::filesystem::is_empty(sources_path));

    if (!has_binaries && !has_sources)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("FMU must contain either a precompiled binary for at least one platform or source code.");
    }

    // Reverse check for FMI 2.0 legacy sources (only if no buildDescription.xml)
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
                static const std::set<std::string> source_extensions = {".c",    ".cc",  ".cpp", ".cxx", ".C",
                                                                        ".c++",  ".cp",  ".cppm", ".ixx"};
                std::string ext = entry.path().extension().string();

                if (source_extensions.contains(ext))
                {
                    if (!listed_sources_in_md.contains(filename))
                    {
                        if (test.status == TestStatus::PASS)
                            test.status = TestStatus::WARNING;
                        test.messages.push_back("Source file '" + filename +
                                                "' exists in 'sources/' directory but is not listed in "
                                                "'modelDescription.xml'.");
                    }
                }
            }
        }
    }

    // FMI 2 compatibility warnings
    if (has_sources)
    {
        bool has_sources_in_md = !listed_sources_in_md.empty();
        if (has_sources_in_md && !has_build_description)
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("FMI 2.0 source code FMU only contains <SourceFiles> in modelDescription.xml. "
                                    "It is recommended to also provide a buildDescription.xml for FMI 2.0.4+ "
                                    "compatibility.");
        }
        else if (!has_sources_in_md && has_build_description)
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("FMI 2.0 source code FMU only contains buildDescription.xml. For backwards "
                                    "compatibility with older FMI 2.0 importers, it is recommended to also provide "
                                    "<SourceFiles> in modelDescription.xml.");
        }
    }

    cert.printTestResult(test);
    cert.printSubsectionSummary(test.status != TestStatus::FAIL);
}
