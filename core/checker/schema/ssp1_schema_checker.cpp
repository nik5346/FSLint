#include "ssp1_schema_checker.h"

#include "file_utils.h"
#include "schema_checker.h"

#include <filesystem>
#include <vector>

std::vector<XmlFileRule> Ssp1SchemaChecker::getXmlRules(const std::filesystem::path& path) const
{
    std::vector<XmlFileRule> rules;

    // 1. SystemStructure.ssd (mandatory)
    rules.push_back({.relative_path = "SystemStructure.ssd",
                     .schema_filename = "SystemStructureDescription.xsd",
                     .is_mandatory = true,
                     .validation_name = "SystemStructure.ssd"});

    // 2. All other .ssd files in root directory
    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".ssd" &&
            entry.path().filename() != "SystemStructure.ssd")
        {
            rules.push_back({.relative_path = entry.path().filename(),
                             .schema_filename = "SystemStructureDescription.xsd",
                             .is_mandatory = false,
                             .validation_name = file_utils::pathToUtf8(entry.path().filename())});
        }
    }

    // 3. All .ssm files (SystemStructureParameterMapping.xsd)
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".ssm")
        {
            auto rel_path = std::filesystem::relative(entry.path(), path);
            rules.push_back({.relative_path = rel_path,
                             .schema_filename = "SystemStructureParameterMapping.xsd",
                             .is_mandatory = false,
                             .validation_name = file_utils::pathToUtf8(entry.path().filename())});
        }
    }

    // 4. All .ssv files (SystemStructureParameterValues.xsd)
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".ssv")
        {
            auto rel_path = std::filesystem::relative(entry.path(), path);
            rules.push_back({.relative_path = rel_path,
                             .schema_filename = "SystemStructureParameterValues.xsd",
                             .is_mandatory = false,
                             .validation_name = file_utils::pathToUtf8(entry.path().filename())});
        }
    }

    // 5. All .ssb files (SystemStructureSignalDictionary.xsd)
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".ssb")
        {
            auto rel_path = std::filesystem::relative(entry.path(), path);
            rules.push_back({.relative_path = rel_path,
                             .schema_filename = "SystemStructureSignalDictionary.xsd",
                             .is_mandatory = false,
                             .validation_name = file_utils::pathToUtf8(entry.path().filename())});
        }
    }

    return rules;
}