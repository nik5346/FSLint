#pragma once

#include "checker.h"

#include <filesystem>
#include <libxml/tree.h>
#include <map>
#include <optional>
#include <set>
#include <string>

/// @brief Base class for validating directory structures of FMUs or SSPs.
class DirectoryChecker : public Checker
{
  public:
    /// @brief Validates directory structure.
    /// @param path Root directory.
    /// @param cert Certificate to record results.
    void validate(const std::filesystem::path& path, Certificate& cert) const override;

  protected:
    /// @brief Hook for version-specific directory checks.
    /// @param path Root directory.
    /// @param cert Certificate to record results.
    /// @param model_identifiers Identifiers.
    /// @param listed_sources_in_md Sources listed in XML.
    /// @param needs_execution_tool True if tool is required.
    virtual void performVersionSpecificChecks(const std::filesystem::path& path, Certificate& cert,
                                              const std::map<std::string, std::string>& model_identifiers,
                                              const std::set<std::string>& listed_sources_in_md,
                                              bool needs_execution_tool) const = 0;

    /// @brief Checks for mandatory header files in sources.
    /// @param path Root directory.
    /// @param cert Certificate to record results.
    /// @param headers Set of expected headers.
    void checkStandardHeaders(const std::filesystem::path& path, Certificate& cert,
                              const std::set<std::string>& headers) const;

    /// @brief Gets XML attribute.
    /// @param node XML node.
    /// @param attr_name Name.
    /// @return Value or std::nullopt.
    static std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);

    /// @brief Checks if directory is effectively empty.
    /// @param path Directory path.
    /// @return True if empty.
    static bool isEffectivelyEmpty(const std::filesystem::path& path);
};
