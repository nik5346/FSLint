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

class TerminalsAndIconsCheckerBase : public Checker
{
  public:
    void validate(const std::filesystem::path& path, Certificate& cert) override;

  protected:
    virtual std::map<std::string, TerminalVariableInfo>
    extractVariables(const std::filesystem::path& path, Certificate& cert, std::string& fmiVersion) = 0;

    std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);

  private:
    void checkTerminalsAndIcons(const std::filesystem::path& path, const std::string& fmiModelDescriptionVersion,
                                const std::map<std::string, TerminalVariableInfo>& variables, Certificate& cert);
};

class Fmi2TerminalsAndIconsChecker : public TerminalsAndIconsCheckerBase
{
  protected:
    std::map<std::string, TerminalVariableInfo> extractVariables(const std::filesystem::path& path, Certificate& cert,
                                                                 std::string& fmiVersion) override;
};

class Fmi3TerminalsAndIconsChecker : public TerminalsAndIconsCheckerBase
{
  protected:
    std::map<std::string, TerminalVariableInfo> extractVariables(const std::filesystem::path& path, Certificate& cert,
                                                                 std::string& fmiVersion) override;
};
