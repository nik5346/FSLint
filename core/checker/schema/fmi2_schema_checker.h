#pragma once

#include "schema_checker.h"

#include <filesystem>
#include <string>
#include <vector>

/// @brief XML schema validator for FMI 2.0.
class Fmi2SchemaChecker : public SchemaCheckerBase
{
  protected:
    /// @brief Gets XML validation rules.
    /// @param path FMU root directory.
    /// @return Rules.
    [[nodiscard]] std::vector<XmlFileRule> getXmlRules(const std::filesystem::path& path) const override;

    /// @brief Gets standard name.
    /// @return "fmi".
    [[nodiscard]] std::string getStandardName() const override
    {
        return "fmi";
    }

    /// @brief Gets standard version.
    /// @return "2.0".
    [[nodiscard]] std::string getStandardVersion() const override
    {
        return "2.0";
    }
};
