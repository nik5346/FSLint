#include "fmi1_schema_checker.h"

std::vector<XmlFileRule> Fmi1MeSchemaChecker::getXmlRules(const std::filesystem::path& /*path*/) const
{
    std::vector<XmlFileRule> rules;

    // 1. modelDescription.xml (mandatory)
    rules.push_back({"modelDescription.xml", "fmiModelDescription.xsd", true, "modelDescription.xml"});

    return rules;
}

std::vector<XmlFileRule> Fmi1CsSchemaChecker::getXmlRules(const std::filesystem::path& /*path*/) const
{
    std::vector<XmlFileRule> rules;

    // 1. modelDescription.xml (mandatory)
    rules.push_back({"modelDescription.xml", "fmiModelDescription.xsd", true, "modelDescription.xml"});

    return rules;
}
