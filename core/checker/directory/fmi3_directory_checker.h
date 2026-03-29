#pragma once

#include "directory_checker.h"

#include "certificate.h"

#include <filesystem>
#include <map>
#include <set>
#include <string>

/// @brief Validator for FMI 3.0 directory structure.
class Fmi3DirectoryChecker : public DirectoryChecker
{
  protected:
    /// @brief FMI 3.0 specific checks.
    /// @param path Root directory.
    /// @param cert Certificate.
    /// @param model_identifiers Identifiers.
    /// @param listed_sources_in_md Sources.
    /// @param needs_execution_tool True if tool required.
    void performVersionSpecificChecks(const std::filesystem::path& path, Certificate& cert,
                                      const std::map<std::string, std::string>& model_identifiers,
                                      const std::set<std::string>& listed_sources_in_md,
                                      bool needs_execution_tool) const override;
};
