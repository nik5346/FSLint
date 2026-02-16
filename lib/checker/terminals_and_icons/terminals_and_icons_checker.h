#pragma once

#include "certificate.h"
#include "checker.h"

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct TerminalDimension
{
    std::optional<uint64_t> start;
    std::optional<uint32_t> value_reference;

    bool operator==(const TerminalDimension& other) const
    {
        return start == other.start && value_reference == other.value_reference;
    }
};

struct TerminalVariableInfo
{
    std::string name;
    std::string causality;
    std::string variability;
    std::string type;
    int sourceline;
    std::vector<TerminalDimension> dimensions;
};

class TerminalsAndIconsCheckerBase : public Checker
{
  public:
    TerminalsAndIconsCheckerBase() = default;
    ~TerminalsAndIconsCheckerBase() override = default;

    // Disable copying and moving to match base class and satisfy rule of five
    TerminalsAndIconsCheckerBase(const TerminalsAndIconsCheckerBase&) = delete;
    TerminalsAndIconsCheckerBase& operator=(const TerminalsAndIconsCheckerBase&) = delete;
    TerminalsAndIconsCheckerBase(TerminalsAndIconsCheckerBase&&) = delete;
    TerminalsAndIconsCheckerBase& operator=(TerminalsAndIconsCheckerBase&&) = delete;

    void validate(const std::filesystem::path& path, Certificate& cert) override;

  protected:
    virtual std::map<std::string, TerminalVariableInfo>
    extractVariables(const std::filesystem::path& path, Certificate& cert, std::string& fmiVersion) = 0;

    bool checkTerminalsAndIcons(const std::filesystem::path& path,
                                const std::map<std::string, TerminalVariableInfo>& variables, Certificate& cert);

    virtual void checkFmiVersion(xmlNodePtr root, TestResult& test) = 0;
    void checkUniqueTerminalNames(xmlXPathContextPtr context, const std::string& p, TestResult& test);
    void checkVariableReferences(xmlXPathContextPtr context, const std::string& p,
                                 const std::map<std::string, TerminalVariableInfo>& variables, TestResult& test);
    void checkUniqueMemberNames(xmlXPathContextPtr context, const std::string& p, TestResult& test);
    void checkStreamFlowConstraints(xmlXPathContextPtr context, const std::string& p, TestResult& test);
    void checkGraphicalRepresentation(const std::filesystem::path& path, xmlXPathContextPtr context,
                                      const std::string& p, TestResult& test);

    std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);
};
