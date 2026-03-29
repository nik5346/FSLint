#pragma once

#include "directory_checker.h"

#include "certificate.h"

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>

/// @brief Validator for FMI 1.0 directory structure.
class Fmi1DirectoryChecker : public DirectoryChecker
{
  public:
    /// @brief Constructor.
    Fmi1DirectoryChecker() = default;

    /// @brief Validates directory structure for FMI 1.0.
    /// @param path Root directory.
    /// @param cert Certificate to record results.
    void validate(const std::filesystem::path& path, Certificate& cert) const override;

  protected:
    /// @brief Performs FMI 1.0 specific checks.
    /// @param path Root directory.
    /// @param cert Certificate to record results.
    /// @param model_identifiers Identifiers.
    /// @param listed_sources_in_md Listed sources.
    /// @param needs_execution_tool True if tool is required.
    void performVersionSpecificChecks(const std::filesystem::path& path, Certificate& cert,
                                      const std::map<std::string, std::string>& model_identifiers,
                                      const std::set<std::string>& listed_sources_in_md,
                                      bool needs_execution_tool) const override;
};
