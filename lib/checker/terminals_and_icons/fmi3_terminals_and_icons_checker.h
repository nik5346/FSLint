#pragma once

#include "terminals_and_icons_checker.h"

#include "certificate.h"

#include <libxml/tree.h>

#include <filesystem>
#include <map>
#include <string>

class Fmi3TerminalsAndIconsChecker : public TerminalsAndIconsCheckerBase
{
  protected:
    std::map<std::string, TerminalVariableInfo> extractVariables(const std::filesystem::path& path, Certificate& cert,
                                                                 std::string& fmiVersion) override;
    void checkFmiVersion(xmlNodePtr root, TestResult& test) override;
};
