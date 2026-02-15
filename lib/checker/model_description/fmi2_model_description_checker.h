#pragma once

#include "model_description_checker.h"

class Fmi2ModelDescriptionChecker : public ModelDescriptionCheckerBase
{
  protected:
    void performVersionSpecificChecks(xmlDocPtr doc, const std::vector<Variable>& variables,
                                      const std::map<std::string, TypeDefinition>& type_definitions,
                                      const std::map<std::string, UnitDefinition>& units, Certificate& cert) override;
    std::string getFmiVersion() const override
    {
        return "2.0";
    }

    // FMI2-specific implementations
    void applyDefaultInitialValues(std::vector<Variable>& variables) override;
    void checkCausalityVariabilityInitialCombinations(const std::vector<Variable>& variables,
                                                      Certificate& cert) override;
    void checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert) override;
    void checkRequiredStartValues(const std::vector<Variable>& variables, Certificate& cert) override;
    void validateFmiVersionValue(const std::string& version, TestResult& test) override;
    void checkIllegalStartValues(const std::vector<Variable>& variables, Certificate& cert) override;
    void checkMinMaxStartValues(const std::vector<Variable>& variables,
                                const std::map<std::string, TypeDefinition>& type_definitions,
                                Certificate& cert) override;

    void checkUnits(xmlDocPtr doc, Certificate& cert) override;
    void checkTypeDefinitions(xmlDocPtr doc, Certificate& cert) override;
    void checkAnnotations(xmlDocPtr doc, Certificate& cert) override;
    void checkGuid(const std::optional<std::string>& guid, Certificate& cert) override;

    void validateVariableSpecialFloat(TestResult& test, const Variable& var, const std::string& val,
                                      const std::string& attr_name) override;
    void validateDefaultExperimentSpecialFloat(TestResult& test, const std::string& val,
                                               const std::string& attr_name) override;
    void validateUnitSpecialFloat(TestResult& test, const std::string& val, const std::string& attr_name,
                                  const std::string& context, size_t line) override;
    void validateTypeDefinitionSpecialFloat(TestResult& test, const TypeDefinition& type_def, const std::string& val,
                                            const std::string& attr_name) override;

  private:
    // FMI2-specific variable extraction (different XML structure than FMI3)
    std::vector<Variable> extractVariables(xmlDocPtr doc) override;
    ModelMetadata extractMetadata(xmlNodePtr root) override;

    // FMI2-specific checks
    std::map<std::string, UnitDefinition> extractUnitDefinitions(xmlDocPtr doc) override;
    std::map<std::string, TypeDefinition> extractTypeDefinitions(xmlDocPtr doc) override;

    // FMI2-specific model structure checks
    void checkModelStructure(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert);
    void validateOutputs(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert);
    void validateDerivatives(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert);
    void validateInitialUnknowns(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert);

    void checkEnumerationVariables(const std::vector<Variable>& variables, Certificate& cert);
    void checkAliases(const std::vector<Variable>& variables, Certificate& cert);
    void checkIndependentVariable(const std::vector<Variable>& variables, Certificate& cert);
    void checkReinitAttribute(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert);
    void checkMultipleSetAttribute(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert);
    void checkContinuousStatesAndDerivatives(const std::vector<Variable>& variables, Certificate& cert);
    void checkSourceFilesSemantic(xmlDocPtr doc, Certificate& cert);
};
