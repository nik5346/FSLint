#include "fmi2_schema_checker.h"

#include "schema_checker.h"

#include <filesystem>
#include <vector>

std::vector<XmlFileRule> Fmi2SchemaChecker::getXmlRules(const std::filesystem::path& path) const
{
    std::vector<XmlFileRule> rules;

    // 1. modelDescription.xml (mandatory)
    rules.push_back({.relative_path = "modelDescription.xml",
                     .schema_filename = "fmi2ModelDescription.xsd",
                     .is_mandatory = true,
                     .validation_name = "modelDescription.xml"});

    // 2. buildDescription.xml (optional)
    if (std::filesystem::exists(path / "sources" / "buildDescription.xml"))
        rules.push_back({.relative_path = "sources/buildDescription.xml",
                         .schema_filename = "fmi2BuildDescription.xsd",
                         .is_mandatory = false,
                         .validation_name = "buildDescription.xml"});

    // 3. terminalsAndIcons.xml (optional)
    if (std::filesystem::exists(path / "terminalsAndIcons" / "terminalsAndIcons.xml"))
    {
        rules.push_back({.relative_path = "terminalsAndIcons/terminalsAndIcons.xml",
                         .schema_filename = "fmi2TerminalsAndIcons.xsd",
                         .is_mandatory = false,
                         .validation_name = "terminalsAndIcons.xml"});
    }

    return rules;
}