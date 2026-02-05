#pragma once

#include "model_description_checker.h"

class Fmi3ModelDescriptionChecker : public ModelDescriptionCheckerBase
{
  protected:
    void performVersionSpecificChecks(xmlDocPtr doc, const std::vector<Variable>& variables,
                                      const std::map<std::string, TypeDefinition>& type_definitions,
                                      const std::map<std::string, UnitDefinition>& units, Certificate& cert) override;
    std::string getFmiVersion() const override
    {
        return "3.0";
    }

    // FMI3-specific implementations
    void applyDefaultInitialValues(std::vector<Variable>& variables) override;
    void checkCausalityVariabilityInitialCombinations(const std::vector<Variable>& variables,
                                                      Certificate& cert) override;
    void checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert) override;
    void checkRequiredStartValues(const std::vector<Variable>& variables, Certificate& cert) override;
    void checkIllegalStartValues(const std::vector<Variable>& variables, Certificate& cert) override;
    void checkMinMaxStartValues(const std::vector<Variable>& variables,
                                const std::map<std::string, TypeDefinition>& type_definitions,
                                Certificate& cert) override;

    void validateVariableSpecialFloat(TestResult& test, const Variable& var, const std::string& val,
                                      const std::string& attr_name) override;
    void validateDefaultExperimentSpecialFloat(TestResult& test, const std::string& val,
                                               const std::string& attr_name) override;
    void validateUnitSpecialFloat(TestResult& test, const std::string& val, const std::string& attr_name,
                                  const std::string& context, size_t line) override;
    void validateTypeDefinitionSpecialFloat(TestResult& test, const TypeDefinition& type_def, const std::string& val,
                                            const std::string& attr_name) override;

  private:
    // FMI3-specific variable extraction (different XML structure than FMI2)
    std::vector<Variable> extractVariables(xmlDocPtr doc) override;
    std::string getVariableType(xmlNodePtr node);

    // Extract dimension information from variable node
    void extractDimensions(xmlNodePtr node, Variable& var);

    // FMI3-specific checks
    void checkIndependentVariable(const std::vector<Variable>& variables, Certificate& cert);
    void checkUniqueValueReferences(const std::vector<Variable>& variables, Certificate& cert);
    void checkStructuralParameter(const std::vector<Variable>& variables, Certificate& cert);

    // New: Check dimension references and array start values
    void checkDimensionReferences(const std::vector<Variable>& variables, Certificate& cert);
    void checkArrayStartValues(const std::vector<Variable>& variables, Certificate& cert);

    void checkClockReferences(const std::vector<Variable>& variables, Certificate& cert);
    void checkClockedVariables(const std::vector<Variable>& variables, Certificate& cert);

    std::map<std::string, TypeDefinition> extractTypeDefinitions(xmlDocPtr doc) override;

    // FMI3-specific model structure checks
    void checkModelStructure(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert);
    void validateOutputs(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert);
    void validateDerivatives(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert);
    void validateInitialUnknowns(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert);
};