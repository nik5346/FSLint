#pragma once

#include "terminals_and_icons_checker.h"

#include "certificate.h"

#include <libxml/tree.h>

#include <filesystem>
#include <map>
#include <string>

/// @brief Validator for Terminals and Icons in FMI 2.0.
class Fmi2TerminalsAndIconsChecker : public TerminalsAndIconsCheckerBase
{
  protected:
    /// @brief Extracts variables.
    /// @param path FMU root directory.
    /// @param cert Certificate to record results.
    /// @param fmi_version Output version string.
    /// @return Map of variables.
    std::map<std::string, TerminalVariableInfo> extractVariables(const std::filesystem::path& path, Certificate& cert,
                                                                 std::string& fmi_version) const override;

    /// @brief Validates terminalsAndIcons.xml version.
    /// @param root XML node.
    /// @param test Result to update.
    void checkFmiVersion(xmlNodePtr root, TestResult& test) const override;
};
