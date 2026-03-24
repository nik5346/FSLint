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

class Fmi1ModelDescriptionChecker : public ModelDescriptionCheckerBase
{
  protected:
    void performVersionSpecificChecks(xmlDocPtr doc, const std::vector<Variable>& variables,
                                      const std::map<std::string, TypeDefinition>& type_definitions,
                                      const std::map<std::string, UnitDefinition>& units,
                                      Certificate& cert) const override;

    std::string getFmiVersion() const override
    {
        return "1.0";
    }

    void validateFmiVersionValue(const std::string& version, TestResult& test) const override;
    void checkGuid(const std::optional<std::string>& guid, Certificate& cert) const override;
    void checkAnnotations(xmlDocPtr doc, Certificate& cert) const override;
    void checkGenerationDateReleaseYear(const std::string& dt, std::time_t generation_time,
                                        TestResult& test) const override;

    void checkModelIdentifier(const std::string& model_identifier, const std::string& interface_name,
                              Certificate& cert) const override;

    void applyDefaultInitialValues(std::vector<Variable>& variables) const override;
    void checkCausalityVariabilityInitialCombinations(const std::vector<Variable>& variables,
                                                      Certificate& cert) const override;
    void checkCausalityVariabilityInitialCombinationsImpl(const std::vector<Variable>& variables, Certificate& cert);
    void checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert) const override;
    void checkLegalVariabilityImpl(const std::vector<Variable>& variables, Certificate& cert);
    void checkRequiredStartValues(const std::vector<Variable>& variables, Certificate& cert) const override;
    void checkIllegalStartValues(const std::vector<Variable>& variables, Certificate& cert) const override;
    void checkMinMaxStartValues(const std::vector<Variable>& variables,
                                const std::map<std::string, TypeDefinition>& type_definitions,
                                Certificate& cert) const override;

    ModelMetadata extractMetadata(xmlNodePtr root) const override;
    std::map<std::string, std::string>
    extractModelIdentifiers(xmlDocPtr doc, const std::vector<std::string>& interface_elements) const override;
    std::map<std::string, UnitDefinition> extractUnitDefinitions(xmlDocPtr doc) const override;
    std::map<std::string, TypeDefinition> extractTypeDefinitions(xmlDocPtr doc) const override;
    std::vector<Variable> extractVariables(xmlDocPtr doc) const override;

    void checkUnits(xmlDocPtr doc, Certificate& cert) const override;
    void checkTypeDefinitions(xmlDocPtr doc, Certificate& cert) const override;

    void validateVariableSpecialFloat(TestResult& test, const Variable& var, const std::string& val,
                                      const std::string& attr_name) const override;
    void validateDefaultExperimentSpecialFloat(TestResult& test, const std::string& val,
                                               const std::string& attr_name) const override;
    void validateUnitSpecialFloat(TestResult& test, const std::string& val, const std::string& attr_name,
                                  const std::string& unit_name, size_t line) const override;
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
