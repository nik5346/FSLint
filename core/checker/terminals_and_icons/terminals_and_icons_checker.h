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

/// @brief Dimension information for terminal variables.
struct TerminalDimension
{
    std::optional<uint64_t> start;           ///< Fixed dimension.
    std::optional<uint32_t> value_reference; ///< Structural parameter reference.
};

/// @brief Equality operator for TerminalDimension.
/// @param lhs Left hand side.
/// @param rhs Right hand side.
/// @return True if equal.
inline bool operator==(const TerminalDimension& lhs, const TerminalDimension& rhs)
{
    return lhs.start == rhs.start && lhs.value_reference == rhs.value_reference;
}

/// @brief Variable metadata used for terminal matching.
struct TerminalVariableInfo
{
    std::string name;                          ///< Name.
    std::string causality;                     ///< Causality.
    std::string variability;                   ///< Variability.
    std::string type;                          ///< Type.
    int sourceline;                            ///< XML line number.
    std::vector<TerminalDimension> dimensions; ///< Dimensions.
};

/// @brief Base class for validating terminalsAndIcons.xml.
class TerminalsAndIconsCheckerBase : public Checker
{
  public:
    /// @brief Constructor.
    TerminalsAndIconsCheckerBase() = default;

    /// @brief Destructor.
    ~TerminalsAndIconsCheckerBase() override = default;

    // Disable copying and moving to match base class and satisfy rule of five
    TerminalsAndIconsCheckerBase(const TerminalsAndIconsCheckerBase&) = delete;
    TerminalsAndIconsCheckerBase& operator=(const TerminalsAndIconsCheckerBase&) = delete;
    TerminalsAndIconsCheckerBase(TerminalsAndIconsCheckerBase&&) = delete;
    TerminalsAndIconsCheckerBase& operator=(TerminalsAndIconsCheckerBase&&) = delete;

    /// @brief Validates terminals and icons.
    /// @param path FMU root.
    /// @param cert Certificate to record results.
    void validate(const std::filesystem::path& path, Certificate& cert) const override;

  protected:
    /// @brief Extracts variables for matching.
    /// @param path FMU root.
    /// @param cert Certificate.
    /// @param fmiVersion Output FMI version.
    /// @return Map of variables.
    virtual std::map<std::string, TerminalVariableInfo>
    extractVariables(const std::filesystem::path& path, Certificate& cert, std::string& fmiVersion) const = 0;

    /// @brief Core validation logic for terminalsAndIcons.xml.
    /// @param path FMU root.
    /// @param variables Extracted variables.
    /// @param cert Certificate.
    /// @return True if file exists and was checked.
    bool checkTerminalsAndIcons(const std::filesystem::path& path,
                                const std::map<std::string, TerminalVariableInfo>& variables, Certificate& cert) const;

    /// @brief Validates version in terminalsAndIcons.xml.
    /// @param root XML node.
    /// @param test Result to update.
    virtual void checkFmiVersion(xmlNodePtr root, TestResult& test) const = 0;

    /// @brief Checks for duplicate terminal names.
    /// @param context XPath context.
    /// @param p XPath prefix.
    /// @param test Result to update.
    void checkUniqueTerminalNames(xmlXPathContextPtr context, const std::string& p, TestResult& test) const;

    /// @brief Validates variable references in terminals.
    /// @param context XPath context.
    /// @param p XPath prefix.
    /// @param variables Extracted variables.
    /// @param test Result to update.
    void checkVariableReferences(xmlXPathContextPtr context, const std::string& p,
                                 const std::map<std::string, TerminalVariableInfo>& variables, TestResult& test) const;

    /// @brief Checks for unique member names within terminals.
    /// @param context XPath context.
    /// @param p XPath prefix.
    /// @param test Result to update.
    void checkUniqueMemberNames(xmlXPathContextPtr context, const std::string& p, TestResult& test) const;

    /// @brief Validates stream/flow constraints.
    /// @param context XPath context.
    /// @param p XPath prefix.
    /// @param test Result to update.
    void checkStreamFlowConstraints(xmlXPathContextPtr context, const std::string& p, TestResult& test) const;

    /// @brief Validates icon and diagram files.
    /// @param path FMU root.
    /// @param context XPath context.
    /// @param p XPath prefix.
    /// @param test Result to update.
    void checkGraphicalRepresentation(const std::filesystem::path& path, xmlXPathContextPtr context,
                                      const std::string& p, TestResult& test) const;

    /// @brief Gets XML attribute.
    /// @param node XML node.
    /// @param attr_name Name.
    /// @return Value or std::nullopt.
    std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name) const;
};
