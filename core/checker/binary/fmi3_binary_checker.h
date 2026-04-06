#pragma once

#include "binary_checker.h"

#include <string>
#include <vector>

/// @brief Validator for FMI 3.0 binaries.
class Fmi3BinaryChecker : public BinaryChecker
{
  protected:
    /// @brief Gets mandatory FMI 3.0 functions.
    /// @return List of functions.
    [[nodiscard]] std::vector<std::string> getExpectedFunctions() const override;
};
