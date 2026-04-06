#include "fmi3_schema_checker.h"

#include "schema_checker.h"

#include "file_utils.h"

#include <filesystem>
#include <vector>

std::vector<XmlFileRule> Fmi3SchemaChecker::getXmlRules(const std::filesystem::path& path) const
{
    std::vector<XmlFileRule> rules;

    // 1. modelDescription.xml (mandatory)
    rules.push_back({.relative_path = "modelDescription.xml",
                     .schema_filename = "fmi3ModelDescription.xsd",
                     .is_mandatory = true,
                     .validation_name = "modelDescription.xml"});

    // 2. buildDescription.xml (optional)
    if (std::filesystem::exists(path / "sources" / "buildDescription.xml"))
        rules.push_back({.relative_path = "sources/buildDescription.xml",
                         .schema_filename = "fmi3BuildDescription.xsd",
                         .is_mandatory = false,
                         .validation_name = "buildDescription.xml"});

    // 3. terminalsAndIcons.xml (optional)
    if (std::filesystem::exists(path / "terminalsAndIcons" / "terminalsAndIcons.xml"))
    {
        rules.push_back({.relative_path = "terminalsAndIcons/terminalsAndIcons.xml",
                         .schema_filename = "fmi3TerminalsAndIcons.xsd",
                         .is_mandatory = false,
                         .validation_name = "terminalsAndIcons.xml"});
    }

    // 4. All fmi-ls-manifest.xml files in extra/ (optional)
    const auto extra_path = path / "extra";
    if (std::filesystem::exists(extra_path))
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(extra_path))
        {
            if (entry.is_regular_file() && entry.path().filename() == "fmi-ls-manifest.xml")
            {
                auto rel_path = std::filesystem::relative(entry.path(), path);
                rules.push_back({.relative_path = file_utils::pathToUtf8(rel_path),
                                 .schema_filename = "fmi3LayeredStandardManifest.xsd",
                                 .is_mandatory = false,
                                 .validation_name = file_utils::pathToUtf8(rel_path)});
            }
        }
    }

    return rules;
}