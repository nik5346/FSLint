#pragma once

#include "checker.h"

#include <filesystem>

/// @brief Validator for resource files.
class ResourcesChecker : public Checker
{
  public:
    /// @brief Validates resources.
    /// @param path Root directory.
    /// @param cert Certificate to record results.
    void validate(const std::filesystem::path& path, Certificate& cert) const override;

  private:
    void scanResources(const std::filesystem::path& resources_dir, Certificate& cert,
                       const std::string& logical_prefix = "") const;
};
