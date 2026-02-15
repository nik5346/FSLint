#pragma once

#include "schema_checker.h"

#include <filesystem>
#include <string>
#include <vector>

class Ssp1SchemaChecker : public SchemaCheckerBase
{
  protected:
    std::vector<XmlFileRule> getXmlRules(const std::filesystem::path& path) const override;
    std::string getStandardName() const override
    {
        return "ssp";
    }
    std::string getStandardVersion() const override
    {
        return "1.0";
    }
};