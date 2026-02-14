#pragma once

#include "schema_checker.h"

class Fmi1MeSchemaChecker : public SchemaCheckerBase
{
  protected:
    std::vector<XmlFileRule> getXmlRules(const std::filesystem::path& path) const override;
    std::string getStandardName() const override
    {
        return "fmi";
    }
    std::string getStandardVersion() const override
    {
        return "1.0/ME";
    }
};

class Fmi1CsSchemaChecker : public SchemaCheckerBase
{
  protected:
    std::vector<XmlFileRule> getXmlRules(const std::filesystem::path& path) const override;
    std::string getStandardName() const override
    {
        return "fmi";
    }
    std::string getStandardVersion() const override
    {
        return "1.0/CS";
    }
};
