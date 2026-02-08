#pragma once

#include "checker.h"
#include <filesystem>
#include <libxml/tree.h>
#include <map>
#include <optional>
#include <string>

struct TerminalVariableInfo
{
    std::string name;
    std::string causality;
    std::string variability;
    std::string type;
    size_t sourceline = 0;
};

class TerminalsAndIconsChecker : public Checker
{
  public:
    void validate(const std::filesystem::path& path, Certificate& cert) override;

  private:
    std::map<std::string, TerminalVariableInfo> extractVariables(const std::filesystem::path& path, Certificate& cert,
                                                                 std::string& fmiVersion);
    void checkTerminalsAndIcons(const std::filesystem::path& path, const std::string& fmiModelDescriptionVersion,
                                const std::map<std::string, TerminalVariableInfo>& variables, Certificate& cert);

    std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);
};
