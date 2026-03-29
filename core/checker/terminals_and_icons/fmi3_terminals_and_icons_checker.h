#pragma once

#include "terminals_and_icons_checker.h"

#include "certificate.h"

#include <libxml/tree.h>

#include <filesystem>
#include <map>
#include <string>

/// @brief Validator for Terminals and Icons in FMI 3.0.
class Fmi3TerminalsAndIconsChecker : public TerminalsAndIconsCheckerBase
{
  protected:
    /// @brief Extracts variables.
    /// @param path FMU root directory.
    /// @param cert Certificate to record results.
    /// @param fmiVersion Output version.
    /// @return Map of variables.
    std::map<std::string, TerminalVariableInfo> extractVariables(const std::filesystem::path& path, Certificate& cert,
                                                                 std::string& fmiVersion) const override;

    /// @brief Validates version.
    /// @param root XML node.
    /// @param test Result to update.
    void checkFmiVersion(xmlNodePtr root, TestResult& test) const override;
};
