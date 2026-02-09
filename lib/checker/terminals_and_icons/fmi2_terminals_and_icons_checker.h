#pragma once
#include "terminals_and_icons_checker.h"

class Fmi2TerminalsAndIconsChecker : public TerminalsAndIconsCheckerBase
{
  protected:
    std::map<std::string, TerminalVariableInfo> extractVariables(const std::filesystem::path& path, Certificate& cert,
                                                                 std::string& fmiVersion) override;
};
