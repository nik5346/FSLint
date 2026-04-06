#pragma once

#include "certificate.h"
#include "checker.h"

#include <filesystem>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <optional>
#include <set>
#include <string>
#include <utility>

/// @brief Base class for validating buildDescription.xml in FMUs.
class BuildDescriptionChecker : public Checker
{
  public:
    /// @brief Constructor.
    /// @param fmi_version Expected FMI version.
    explicit BuildDescriptionChecker(std::string fmi_version)
        : _fmi_version(std::move(fmi_version))
    {
    }

    /// @brief Validates the buildDescription.xml if present.
    /// @param path FMU root directory.
    /// @param cert Certificate to record results.
    void validate(const std::filesystem::path& path, Certificate& cert) const override;

  protected:
    /// @brief Gets the configured FMI version.
    /// @return Version string.
    [[nodiscard]] const std::string& getFmiVersion() const
    {
        return _fmi_version;
    }

    /// @brief Extracts a string attribute from an XML node.
    /// @param node XML node.
    /// @param attr_name Name of the attribute.
    /// @return Attribute value or std::nullopt.
    std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name) const;

    /// @brief Hook for version-specific root validation.
    /// @param root XML root node.
    /// @param cert Certificate to record results.
    virtual void checkFmiVersion(xmlNodePtr root, Certificate& cert) const = 0;

    /// @brief Checks source files listed in buildDescription.xml.
    /// @param xpath_context libxml2 XPath context.
    /// @param sources_path Path to the sources directory.
    /// @param cert Certificate to record results.
    /// @param listed_files Output set of listed source files.
    void checkSourceFiles(xmlXPathContextPtr xpath_context, const std::filesystem::path& sources_path,
                          Certificate& cert, std::set<std::string>& listed_files) const;

    /// @brief Checks include directories listed in buildDescription.xml.
    /// @param xpath_context libxml2 XPath context.
    /// @param sources_path Path to the sources directory.
    /// @param cert Certificate to record results.
    void checkIncludeDirectories(xmlXPathContextPtr xpath_context, const std::filesystem::path& sources_path,
                                 Certificate& cert) const;

    /// @brief Checks build configuration attributes.
    /// @param xpath_context libxml2 XPath context.
    /// @param valid_ids Set of valid model identifiers from modelDescription.xml.
    /// @param cert Certificate to record results.
    void checkBuildConfigurationAttributes(xmlXPathContextPtr xpath_context, const std::set<std::string>& valid_ids,
                                           Certificate& cert) const;

    /// @brief Gets all model identifiers from modelDescription.xml.
    /// @param path FMU root directory.
    /// @return Set of identifiers.
    [[nodiscard]] std::set<std::string> getValidModelIdentifiers(const std::filesystem::path& path) const;

  private:
    std::string _fmi_version;
};
