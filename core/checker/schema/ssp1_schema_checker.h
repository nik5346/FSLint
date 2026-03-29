#pragma once

#include "schema_checker.h"

#include <filesystem>
#include <string>
#include <vector>

/// @brief XML schema validator for SSP 1.0.
class Ssp1SchemaChecker : public SchemaCheckerBase
{
  protected:
    /// @brief Gets validation rules.
    /// @param path Model root.
    /// @return Rules.
    std::vector<XmlFileRule> getXmlRules(const std::filesystem::path& path) const override;

    /// @brief Gets standard name.
    /// @return "ssp".
    std::string getStandardName() const override
    {
        return "ssp";
    }

    /// @brief Gets version.
    /// @return "1.0".
    std::string getStandardVersion() const override
    {
        return "1.0";
    }
};
