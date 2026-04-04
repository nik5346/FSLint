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

/// @brief Semantic validator for FMI 2.0.
class Fmi2ModelDescriptionChecker : public ModelDescriptionCheckerBase
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
    /// @return "2.0".
    std::string getFmiVersion() const override
    {
        return "2.0";
    }

    // FMI2-specific implementations
    void applyDefaultInitialValues(std::vector<Variable>& variables) const override;
    void checkCausalityVariabilityInitialCombinations(const std::vector<Variable>& variables,
                                                      Certificate& cert) const override;
    void checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert) const override;
    void checkRequiredStartValues(const std::vector<Variable>& variables, Certificate& cert) const override;
    void validateFmiVersionValue(const std::string& version, TestResult& test) const override;
    void checkIllegalStartValues(const std::vector<Variable>& variables, Certificate& cert) const override;
    void checkMinMaxStartValues(const std::vector<Variable>& variables,
                                const std::map<std::string, TypeDefinition>& type_definitions, Certificate& cert) const;

    void checkUnits(xmlDocPtr doc, Certificate& cert) const override;
    void checkTypeDefinitions(xmlDocPtr doc, Certificate& cert) const override;
    void checkAnnotations(xmlDocPtr doc, Certificate& cert) const override;
    void checkGuid(const std::optional<std::string>& guid, Certificate& cert) const override;
    void checkGenerationDateReleaseYear(const std::string& dt, std::time_t generation_time,
                                        TestResult& test) const override;

    void validateVariableSpecialFloat(TestResult& test, const Variable& var, const std::string& val,
                                      const std::string& attr_name) const override;
    void validateDefaultExperimentSpecialFloat(TestResult& test, const std::string& val,
                                               const std::string& attr_name) const override;
    void validateUnitSpecialFloat(TestResult& test, const std::string& val, const std::string& attr_name,
                                  const std::string& context, size_t line) const override;
    void validateTypeDefinitionSpecialFloat(TestResult& test, const TypeDefinition& type_def, const std::string& val,
                                            const std::string& attr_name) const override;

  private:
    // FMI2-specific variable extraction (different XML structure than FMI3)
    std::vector<Variable> extractVariables(xmlDocPtr doc) const override;
    ModelMetadata extractMetadata(xmlNodePtr root) const override;

    // FMI2-specific checks
    std::map<std::string, UnitDefinition> extractUnitDefinitions(xmlDocPtr doc) const override;
    std::map<std::string, TypeDefinition> extractTypeDefinitions(xmlDocPtr doc) const override;

    // FMI2-specific model structure checks
    void checkModelStructure(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert) const;
    void validateOutputs(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert) const;
    void validateDerivatives(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert) const;
    void validateInitialUnknowns(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert) const;

    void checkEnumerationVariables(const std::vector<Variable>& variables, Certificate& cert) const;
    void checkAliases(const std::vector<Variable>& variables, Certificate& cert) const;
    void checkIndependentVariable(const std::vector<Variable>& variables, Certificate& cert) const;
    void checkReinitAttribute(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert) const;
    void checkMultipleSetAttribute(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert) const;
    void checkContinuousStatesAndDerivatives(const std::vector<Variable>& variables, Certificate& cert) const;
    void checkSourceFilesSemantic(xmlDocPtr doc, Certificate& cert) const;
};
