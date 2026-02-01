#pragma once

#include "schema_checker.h"

class Fmi3SchemaChecker : public SchemaCheckerBase
{
  protected:
    std::vector<XmlFileRule> getXmlRules(const std::filesystem::path& path) const override;
    std::string getStandardName() const override
    {
        return "fmi";
    }
    std::string getStandardVersion() const override
    {
        return "3.0";
    }
};