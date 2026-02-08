#pragma once
#include "certificate.h"
#include "checker.h"
#include <filesystem>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct TerminalVariableInfo
{
    std::string name;
    std::string causality;
    std::string variability;
    std::string type;
    int sourceline;
};

class TerminalsAndIconsCheckerBase : public Checker
{
  public:
    virtual ~TerminalsAndIconsCheckerBase() = default;
    void validate(const std::filesystem::path& path, Certificate& cert) override;

  protected:
    virtual std::map<std::string, TerminalVariableInfo>
    extractVariables(const std::filesystem::path& path, Certificate& cert, std::string& fmiVersion) = 0;

    bool checkTerminalsAndIcons(const std::filesystem::path& path,
                                const std::map<std::string, TerminalVariableInfo>& variables, Certificate& cert);

    void checkFmiVersion(xmlNodePtr root, TestResult& test);
    void checkUniqueTerminalNames(xmlXPathContextPtr context, const std::string& p, TestResult& test);
    void checkVariableReferences(xmlXPathContextPtr context, const std::string& p,
                                 const std::map<std::string, TerminalVariableInfo>& variables, TestResult& test);
    void checkUniqueMemberNames(xmlXPathContextPtr context, const std::string& p, TestResult& test);
    void checkStreamFlowConstraints(xmlXPathContextPtr context, const std::string& p, TestResult& test);

    std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);
};
