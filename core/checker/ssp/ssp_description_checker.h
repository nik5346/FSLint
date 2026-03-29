#pragma once

#include "checker.h"

#include <filesystem>
#include <string>

/// @brief Validator for SSP description files (SystemStructure.ssd).
class SspDescriptionChecker : public Checker
{
  public:
    /// @brief Validates the SSD file.
    /// @param path Model root directory.
    /// @param cert Certificate to record results.
    void validate(const std::filesystem::path& path, Certificate& cert) const override;
};
