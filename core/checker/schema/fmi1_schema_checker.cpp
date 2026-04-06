#include "fmi1_schema_checker.h"

#include "schema_checker.h"

#include <filesystem>
#include <vector>

std::vector<XmlFileRule> Fmi1MeSchemaChecker::getXmlRules(const std::filesystem::path& /*path*/) const
{
    std::vector<XmlFileRule> rules;

    // 1. modelDescription.xml (mandatory)
    rules.push_back({.relative_path = "modelDescription.xml",
                     .schema_filename = "fmiModelDescription.xsd",
                     .is_mandatory = true,
                     .validation_name = "modelDescription.xml"});

    return rules;
}

std::vector<XmlFileRule> Fmi1CsSchemaChecker::getXmlRules(const std::filesystem::path& /*path*/) const
{
    std::vector<XmlFileRule> rules;

    // 1. modelDescription.xml (mandatory)
    rules.push_back({.relative_path = "modelDescription.xml",
                     .schema_filename = "fmiModelDescription.xsd",
                     .is_mandatory = true,
                     .validation_name = "modelDescription.xml"});

    return rules;
}
