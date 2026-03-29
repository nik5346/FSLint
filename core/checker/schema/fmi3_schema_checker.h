#pragma once

#include "schema_checker.h"

#include <filesystem>
#include <string>
#include <vector>

/// @brief XML schema validator for FMI 3.0.
class Fmi3SchemaChecker : public SchemaCheckerBase
{
  protected:
    /// @brief Gets XML validation rules.
    /// @param path FMU root directory.
    /// @return Rules.
    std::vector<XmlFileRule> getXmlRules(const std::filesystem::path& path) const override;

    /// @brief Gets standard name.
    /// @return "fmi".
    std::string getStandardName() const override
    {
        return "fmi";
    }

    /// @brief Gets standard version.
    /// @return "3.0".
    std::string getStandardVersion() const override
    {
        return "3.0";
    }
};
