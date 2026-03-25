#pragma once

#include "model_description_checker.h"

#include "certificate.h"

#include "libxml/tree.h"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

class Fmi3ModelDescriptionChecker : public ModelDescriptionCheckerBase
{
  protected:
    void performVersionSpecificChecks(xmlDocPtr doc, const std::vector<Variable>& variables,
                                      const std::map<std::string, TypeDefinition>& type_definitions,
                                      const std::map<std::string, UnitDefinition>& units,
                                      Certificate& cert) const override;
    std::string getFmiVersion() const override
    {
        return "3.0";
    }

    // FMI3-specific implementations
    void applyDefaultInitialValues(std::vector<Variable>& variables) const override;
    void checkCausalityVariabilityInitialCombinations(const std::vector<Variable>& variables,
                                                      Certificate& cert) const override;
    void checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert) const override;
    void checkRequiredStartValues(const std::vector<Variable>& variables, Certificate& cert) const override;
    void validateFmiVersionValue(const std::string& version, TestResult& test) const override;
    void checkIllegalStartValues(const std::vector<Variable>& variables, Certificate& cert) const override;
    void checkMinMaxStartValues(const std::vector<Variable>& variables,
                                const std::map<std::string, TypeDefinition>& type_definitions,
                                Certificate& cert) const override;

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
    // FMI3-specific variable extraction (different XML structure than FMI2)
    std::vector<Variable> extractVariables(xmlDocPtr doc) const override;
    ModelMetadata extractMetadata(xmlNodePtr root) const override;
    std::string getVariableType(xmlNodePtr node) const;

    // Extract dimension information from variable node
    void extractDimensions(xmlNodePtr node, Variable& var) const;

    // FMI3-specific checks
    void checkEnumerationVariables(const std::vector<Variable>& variables, Certificate& cert) const;
    void checkIndependentVariable(const std::vector<Variable>& variables, Certificate& cert) const;
    void checkUniqueValueReferences(const std::vector<Variable>& variables, Certificate& cert) const;
    void checkStructuralParameter(const std::vector<Variable>& variables, Certificate& cert) const;

    // New: Check dimension references and array start values
    void checkDimensionReferences(const std::vector<Variable>& variables, Certificate& cert) const;
    void checkArrayStartValues(const std::vector<Variable>& variables, Certificate& cert) const;

    void checkClockReferences(const std::vector<Variable>& variables, Certificate& cert) const;
    void checkClockedVariables(const std::vector<Variable>& variables, Certificate& cert) const;
    void checkAliases(const std::vector<Variable>& variables, Certificate& cert) const;
    void checkReinitAttribute(const std::vector<Variable>& variables, Certificate& cert) const;
    void checkDerivativeConsistency(const std::vector<Variable>& variables, Certificate& cert) const;
    void checkCanHandleMultipleSet(const std::vector<Variable>& variables, Certificate& cert) const;
    void checkClockTypes(xmlDocPtr doc, Certificate& cert) const;

    void checkDerivativeDimensions(const std::vector<Variable>& variables, Certificate& cert) const;
    bool compareDimensions(const Variable& var1, const Variable& var2) const;
    std::string formatDimensions(const Variable& var) const;

    std::map<std::string, UnitDefinition> extractUnitDefinitions(xmlDocPtr doc) const override;
    std::map<std::string, TypeDefinition> extractTypeDefinitions(xmlDocPtr doc) const override;

    // FMI3-specific model structure checks
    void checkModelStructure(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert) const;
    void validateOutputs(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert) const;
    void validateDerivatives(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert) const;
    void validateClockedStates(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert) const;
    void validateInitialUnknowns(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert) const;
    void validateEventIndicators(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert) const;
    void checkVariableDependencies(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert) const;
};