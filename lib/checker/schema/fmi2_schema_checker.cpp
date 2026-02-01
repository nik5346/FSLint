#include "fmi2_schema_checker.h"

std::vector<XmlFileRule> Fmi2SchemaChecker::getXmlRules(const std::filesystem::path& path) const
{
    std::vector<XmlFileRule> rules;

    // 1. modelDescription.xml (mandatory)
    rules.push_back({"modelDescription.xml", "fmi2ModelDescription.xsd", true, "modelDescription.xml"});

    // 2. buildDescription.xml (optional)
    if (std::filesystem::exists(path / "sources" / "buildDescription.xml"))
        rules.push_back({"sources/buildDescription.xml", "fmi3BuildDescription.xsd", false, "buildDescription.xml"});

    return rules;
}