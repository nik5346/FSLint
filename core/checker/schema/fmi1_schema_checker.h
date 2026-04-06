#pragma once

#include "schema_checker.h"

#include <filesystem>
#include <string>
#include <vector>

/// @brief XML schema validator for FMI 1.0 ME.
class Fmi1MeSchemaChecker : public SchemaCheckerBase
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
    /// @return "1.0/ME".
    [[nodiscard]] std::string getStandardVersion() const override
    {
        return "1.0/ME";
    }

    /// @brief UTF-8 requirement.
    /// @return False.
    [[nodiscard]] bool isUtf8Required() const override
    {
        return false;
    }
};

/// @brief XML schema validator for FMI 1.0 CS.
class Fmi1CsSchemaChecker : public SchemaCheckerBase
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
    /// @return "1.0/CS".
    [[nodiscard]] std::string getStandardVersion() const override
    {
        return "1.0/CS";
    }

    /// @brief UTF-8 requirement.
    /// @return False.
    [[nodiscard]] bool isUtf8Required() const override
    {
        return false;
    }
};
