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
    void checkIllegalStartValues(const std::vector<Variable>& variables, Certificate& cert) override;
    void checkMinMaxStartValues(const std::vector<Variable>& variables,
                                const std::map<std::string, TypeDefinition>& type_definitions,
                                Certificate& cert) override;

  private:
    // FMI2-specific variable extraction (different XML structure than FMI3)
    std::vector<Variable> extractVariables(xmlDocPtr doc) override;

    // FMI2-specific checks
    std::map<std::string, TypeDefinition> extractTypeDefinitions(xmlDocPtr doc) override;

    // FMI2-specific model structure checks
    void checkModelStructure(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert);
    void validateOutputs(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert);
    void validateDerivatives(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert);
    void validateInitialUnknowns(xmlDocPtr doc, const std::vector<Variable>& variables, Certificate& cert);
};
