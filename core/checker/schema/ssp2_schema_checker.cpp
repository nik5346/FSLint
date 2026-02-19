#include "ssp2_schema_checker.h"

#include "schema_checker.h"

#include <filesystem>
#include <vector>

std::vector<XmlFileRule> Ssp2SchemaChecker::getXmlRules(const std::filesystem::path& path) const
{
    std::vector<XmlFileRule> rules;

    // 1. SystemStructure.ssd (mandatory)
    rules.push_back({"SystemStructure.ssd", "SystemStructureDescription.xsd", true, "SystemStructure.ssd"});

    // 2. All other .ssd files in root directory
    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".ssd" &&
            entry.path().filename() != "SystemStructure.ssd")
        {
            rules.push_back(
                {entry.path().filename(), "SystemStructureDescription.xsd", false, entry.path().filename().string()});
        }
    }

    // 3. All .ssm files (SystemStructureParameterMapping.xsd)
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".ssm")
        {
            auto rel_path = std::filesystem::relative(entry.path(), path);
            rules.push_back({rel_path, "SystemStructureParameterMapping.xsd", false, entry.path().filename().string()});
        }
    }

    // 4. All .ssv files (SystemStructureParameterValues.xsd)
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".ssv")
        {
            auto rel_path = std::filesystem::relative(entry.path(), path);
            rules.push_back({rel_path, "SystemStructureParameterValues.xsd", false, entry.path().filename().string()});
        }
    }

    // 5. All .ssb files (SystemStructureSignalDictionary.xsd)
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".ssb")
        {
            auto rel_path = std::filesystem::relative(entry.path(), path);
            rules.push_back({rel_path, "SystemStructureSignalDictionary.xsd", false, entry.path().filename().string()});
        }
    }

    return rules;
}