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
    std::vector<XmlFileRule> getXmlRules(const std::filesystem::path& path) const override;

    /// @brief Gets standard name.
    /// @return "fmi".
    std::string getStandardName() const override
    {
        return "fmi";
    }

    /// @brief Gets standard version.
    /// @return "1.0/ME".
    std::string getStandardVersion() const override
    {
        return "1.0/ME";
    }

    /// @brief UTF-8 requirement.
    /// @return False.
    bool isUtf8Required() const override
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
    std::vector<XmlFileRule> getXmlRules(const std::filesystem::path& path) const override;

    /// @brief Gets standard name.
    /// @return "fmi".
    std::string getStandardName() const override
    {
        return "fmi";
    }

    /// @brief Gets standard version.
    /// @return "1.0/CS".
    std::string getStandardVersion() const override
    {
        return "1.0/CS";
    }

    /// @brief UTF-8 requirement.
    /// @return False.
    bool isUtf8Required() const override
    {
        return false;
    }
};
