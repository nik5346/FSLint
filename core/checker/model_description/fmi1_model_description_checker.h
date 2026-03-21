#pragma once

#include "model_description_checker.h"

#include "certificate.h"

#include <libxml/tree.h>

#include <cstddef>
#include <map>
#include <optional>
#include <vector>

struct Variable;

class Fmi1ModelDescriptionChecker : public ModelDescriptionCheckerBase
{
  protected:
    void performVersionSpecificChecks(xmlDocPtr doc, const std::vector<Variable>& variables,
                                      const std::map<std::string, TypeDefinition>& type_definitions,
                                      const std::map<std::string, UnitDefinition>& units, Certificate& cert) override;

    std::string getFmiVersion() const override
    {
        return "1.0";
    }

    void validateFmiVersionValue(const std::string& version, TestResult& test) override;
    void checkGuid(const std::optional<std::string>& guid, Certificate& cert) override;
    void checkAnnotations(xmlDocPtr doc, Certificate& cert) override;
    void checkGenerationDateReleaseYear(const std::string& dt, std::chrono::sys_seconds generation_time,
                                        TestResult& test) override;

    void checkModelIdentifier(const std::string& model_identifier, const std::string& interface_name,
                              Certificate& cert) override;

    void applyDefaultInitialValues(std::vector<Variable>& variables) override;
    void checkCausalityVariabilityInitialCombinations(const std::vector<Variable>& variables,
                                                      Certificate& cert) override;
    void checkCausalityVariabilityInitialCombinationsImpl(const std::vector<Variable>& variables, Certificate& cert);
    void checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert) override;
    void checkLegalVariabilityImpl(const std::vector<Variable>& variables, Certificate& cert);
    void checkRequiredStartValues(const std::vector<Variable>& variables, Certificate& cert) override;
    void checkIllegalStartValues(const std::vector<Variable>& variables, Certificate& cert) override;
    void checkMinMaxStartValues(const std::vector<Variable>& variables,
                                const std::map<std::string, TypeDefinition>& type_definitions,
                                Certificate& cert) override;

    ModelMetadata extractMetadata(xmlNodePtr root) override;
    std::map<std::string, std::string>
    extractModelIdentifiers(xmlDocPtr doc, const std::vector<std::string>& interface_elements) override;
    std::map<std::string, UnitDefinition> extractUnitDefinitions(xmlDocPtr doc) override;
    std::map<std::string, TypeDefinition> extractTypeDefinitions(xmlDocPtr doc) override;
    std::vector<Variable> extractVariables(xmlDocPtr doc) override;

    void checkUnits(xmlDocPtr doc, Certificate& cert) override;
    void checkTypeDefinitions(xmlDocPtr doc, Certificate& cert) override;

    void validateVariableSpecialFloat(TestResult& test, const Variable& var, const std::string& val,
                                      const std::string& attr_name) override;
    void validateDefaultExperimentSpecialFloat(TestResult& test, const std::string& val,
                                               const std::string& attr_name) override;
    void validateUnitSpecialFloat(TestResult& test, const std::string& val, const std::string& attr_name,
                                  const std::string& unit_name, size_t line) override;
    void validateTypeDefinitionSpecialFloat(TestResult& test, const TypeDefinition& type_def, const std::string& val,
                                            const std::string& attr_name) override;

  private:
    void checkModelIdentifierMatch(const std::string& model_identifier, Certificate& cert);
    void checkImplementation(xmlDocPtr doc, Certificate& cert);
    void checkUri(const std::string& uri, const std::string& attr_name, int line, TestResult& test);
    void checkAliases(const std::vector<Variable>& variables, Certificate& cert);
    bool checkReachability(const std::string& url);

    bool _is_cs = false;
};
