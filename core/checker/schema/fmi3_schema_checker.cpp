#include "fmi3_schema_checker.h"

#include "schema_checker.h"

#include <filesystem>
#include <vector>

std::vector<XmlFileRule> Fmi3SchemaChecker::getXmlRules(const std::filesystem::path& path) const
{
    std::vector<XmlFileRule> rules;

    // 1. modelDescription.xml (mandatory)
    rules.push_back({"modelDescription.xml", "fmi3ModelDescription.xsd", true, "modelDescription.xml"});

    // 2. buildDescription.xml (optional)
    if (std::filesystem::exists(path / "sources" / "buildDescription.xml"))
        rules.push_back({"sources/buildDescription.xml", "fmi3BuildDescription.xsd", false, "buildDescription.xml"});

    // 3. terminalsAndIcons.xml (optional)
    if (std::filesystem::exists(path / "terminalsAndIcons" / "terminalsAndIcons.xml"))
    {
        rules.push_back(
            {"terminalsAndIcons/terminalsAndIcons.xml", "fmi3TerminalsAndIcons.xsd", false, "terminalsAndIcons.xml"});
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
                rules.push_back({rel_path, "fmi3LayeredStandardManifest.xsd", false, rel_path.string()});
            }
        }
    }

    return rules;
}