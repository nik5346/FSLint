#pragma once

#include "model_description_checker.h"

#include "certificate.h"

#include <libxml/tree.h>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct Variable;

/// @brief Semantic validator for FMI 1.0.
class Fmi1ModelDescriptionChecker : public ModelDescriptionCheckerBase
{
  protected:
    /// @brief Performs version-specific checks.
    /// @param doc XML document.
    /// @param variables Extracted variables.
    /// @param type_definitions Map of types.
    /// @param units Map of units.
    /// @param cert Certificate to record results.
    void performVersionSpecificChecks(xmlDocPtr doc, const std::vector<Variable>& variables,
                                      const std::map<std::string, TypeDefinition>& type_definitions,
                                      const std::map<std::string, UnitDefinition>& units,
                                      Certificate& cert) const override;

    /// @brief Gets FMI version.
    /// @return "1.0".
    std::string getFmiVersion() const override
    {
        return "1.0";
    }

    /// @brief Validates version value.
    /// @param version Version string.
    /// @param test Result.
    void validateFmiVersionValue(const std::string& version, TestResult& test) const override;

    /// @brief Validates GUID.
    /// @param guid GUID string.
    /// @param cert Certificate.
    void checkGuid(const std::optional<std::string>& guid, Certificate& cert) const override;

    /// @brief Validates annotations.
    /// @param doc XML doc.
    /// @param cert Certificate.
    void checkAnnotations(xmlDocPtr doc, Certificate& cert) const override;

    /// @brief Validates generation year.
    /// @param dt Date string.
    /// @param generation_time Unix time.
    /// @param test Result.
    void checkGenerationDateReleaseYear(const std::string& dt, std::time_t generation_time,
                                        TestResult& test) const override;

    /// @brief Validates model identifier.
    /// @param model_identifier ID.
    /// @param interface_name Interface.
    /// @param cert Certificate.
    void checkModelIdentifier(const std::string& model_identifier, const std::string& interface_name,
                              Certificate& cert) const override;

    /// @brief Applies default initials.
    /// @param variables Variables.
    void applyDefaultInitialValues(std::vector<Variable>& variables) const override;

    /// @brief Checks causality/variability/initial.
    /// @param variables Variables.
    /// @param cert Certificate.
    void checkCausalityVariabilityInitialCombinations(const std::vector<Variable>& variables,
                                                      Certificate& cert) const override;

    /// @brief Implementation of causality checks.
    /// @param variables Variables.
    /// @param cert Certificate.
    void checkCausalityVariabilityInitialCombinationsImpl(const std::vector<Variable>& variables, Certificate& cert);

    /// @brief Checks legal variability.
    /// @param variables Variables.
    /// @param cert Certificate.
    void checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert) const override;

    /// @brief Implementation of variability checks.
    /// @param variables Variables.
    /// @param cert Certificate.
    void checkLegalVariabilityImpl(const std::vector<Variable>& variables, Certificate& cert);

    /// @brief Checks required start values.
    /// @param variables Variables.
    /// @param cert Certificate.
    void checkRequiredStartValues(const std::vector<Variable>& variables, Certificate& cert) const override;

    /// @brief Checks illegal start values.
    /// @param variables Variables.
    /// @param cert Certificate.
    void checkIllegalStartValues(const std::vector<Variable>& variables, Certificate& cert) const override;

    /// @brief Checks min/max start values.
    /// @param variables Variables.
    /// @param type_definitions Types.
    /// @param cert Certificate.
    void checkMinMaxStartValues(const std::vector<Variable>& variables,
                                const std::map<std::string, TypeDefinition>& type_definitions,
                                Certificate& cert) const override;

    /// @brief Extracts metadata.
    /// @param root XML root.
    /// @return Metadata.
    ModelMetadata extractMetadata(xmlNodePtr root) const override;

    /// @brief Extracts identifiers.
    /// @param doc XML doc.
    /// @param interface_elements Elements.
    /// @return Identifiers.
    std::map<std::string, std::string>
    extractModelIdentifiers(xmlDocPtr doc, const std::vector<std::string>& interface_elements) const override;

    /// @brief Extracts units.
    /// @param doc XML doc.
    /// @return Units.
    std::map<std::string, UnitDefinition> extractUnitDefinitions(xmlDocPtr doc) const override;

    /// @brief Extracts types.
    /// @param doc XML doc.
    /// @return Types.
    std::map<std::string, TypeDefinition> extractTypeDefinitions(xmlDocPtr doc) const override;

    /// @brief Extracts variables.
    /// @param doc XML doc.
    /// @return Variables.
    std::vector<Variable> extractVariables(xmlDocPtr doc) const override;

    /// @brief Checks units.
    /// @param doc XML doc.
    /// @param cert Certificate.
    void checkUnits(xmlDocPtr doc, Certificate& cert) const override;

    /// @brief Checks types.
    /// @param doc XML doc.
    /// @param cert Certificate.
    void checkTypeDefinitions(xmlDocPtr doc, Certificate& cert) const override;

    /// @brief Validates special float.
    /// @param test Result.
    /// @param var Variable.
    /// @param val Value.
    /// @param attr_name Attribute.
    void validateVariableSpecialFloat(TestResult& test, const Variable& var, const std::string& val,
                                      const std::string& attr_name) const override;

    /// @brief Validates DefaultExperiment special float.
    /// @param test Result.
    /// @param val Value.
    /// @param attr_name Attribute.
    void validateDefaultExperimentSpecialFloat(TestResult& test, const std::string& val,
                                               const std::string& attr_name) const override;

    /// @brief Validates unit special float.
    /// @param test Result.
    /// @param val Value.
    /// @param attr_name Attribute.
    /// @param unit_name Unit.
    /// @param line Line.
    void validateUnitSpecialFloat(TestResult& test, const std::string& val, const std::string& attr_name,
                                  const std::string& unit_name, size_t line) const override;

    /// @brief Validates type special float.
    /// @param test Result.
    /// @param type_def Type.
    /// @param val Value.
    /// @param attr_name Attribute.
    void validateTypeDefinitionSpecialFloat(TestResult& test, const TypeDefinition& type_def, const std::string& val,
                                            const std::string& attr_name) const override;

  private:
    void checkModelIdentifierMatch(const std::string& model_identifier, Certificate& cert) const;
    void checkImplementation(xmlDocPtr doc, Certificate& cert) const;
    void checkUri(const std::string& uri, const std::string& attr_name, int line, TestResult& test) const;
    void checkAliases(const std::vector<Variable>& variables, Certificate& cert) const;
    bool checkReachability(const std::string& url) const;

    mutable bool _is_cs = false;
};
