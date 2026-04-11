#pragma once

#include "binary_checker.h"

#include <string>
#include <vector>

/// @brief Validator for FMI 2.0 binaries.
class Fmi2BinaryChecker : public BinaryChecker
{
  protected:
    /// @brief Gets mandatory FMI 2.0 functions.
    /// @param interfaces The set of supported interface types.
    /// @return List of function names.
    [[nodiscard]] std::vector<std::string>
    getExpectedFunctions(const std::set<InterfaceType>& interfaces) const override;
};
