#pragma once

#include "binary_checker.h"

#include "certificate.h"

#include <filesystem>
#include <set>
#include <string>
#include <vector>

/// @brief Validator for FMI 1.0 binaries.
class Fmi1BinaryChecker : public BinaryChecker
{
  public:
    /// @brief Validates FMI 1.0 binaries.
    /// @param path FMU root directory.
    /// @param cert Certificate to record results.
    void validate(const std::filesystem::path& path, Certificate& cert) const override;

  protected:
    /// @brief Gets expected functions (not used for FMI 1.0).
    /// @param interfaces Supported interfaces.
    /// @return Empty vector.
    [[nodiscard]] std::vector<std::string>
    getExpectedFunctions(const std::set<InterfaceType>& interfaces) const override
    {
        (void)interfaces;
        return {}; // Not used because we override validate
    }
};
