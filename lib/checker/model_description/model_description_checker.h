#pragma once

#include "checker.h"

#include "certificate.h"

#include <libxml/tree.h>
#include <libxml/xpath.h>

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

// Dimension information for array variables
struct Dimension
{
    std::optional<uint64_t> start;           // Fixed dimension size
    std::optional<uint32_t> value_reference; // Reference to structural parameter
    size_t sourceline = 0;
};

// Variable information extracted from modelDescription.xml
struct Variable
{
    std::string name;
    std::string type;
    std::string causality;
    std::string variability;
    std::string initial;
    std::optional<std::string> start;
    size_t num_start_values = 0;
    std::optional<std::string> min;
    std::optional<std::string> max;
    std::optional<std::string> nominal;
    std::optional<std::string> unit;
    std::optional<std::string> display_unit;
    std::optional<std::string> declared_type;
    std::optional<uint32_t> value_reference;
    std::optional<uint32_t> derivative_of;
    std::optional<bool> reinit;
    std::optional<bool> can_handle_multiple_set;
    uint32_t index = 0; // 1-based ScalarVariable index
    bool is_alias = false;
    bool has_dimension = false;
    std::vector<Dimension> dimensions; // Full dimension information
    std::optional<std::string> clocks;
    bool relative_quantity = false;
    size_t sourceline = 0;
};

// Display unit definition
struct DisplayUnit
{
    std::string name;
    std::optional<std::string> factor;
    std::optional<std::string> offset;
    size_t sourceline = 0;
};

// Unit definition
struct UnitDefinition
{
    std::string name;
    std::optional<std::string> factor;
    std::optional<std::string> offset;
    std::map<std::string, DisplayUnit> display_units;
    size_t sourceline = 0;
};

// Type definition
struct TypeDefinition
{
    std::string name;
    std::string type;
    std::optional<std::string> min;
    std::optional<std::string> max;
    std::optional<std::string> nominal;
    std::optional<std::string> unit;
    std::optional<std::string> display_unit;
    bool relative_quantity = false;
    size_t sourceline = 0;
};

// Common metadata from the root element of modelDescription.xml
struct ModelMetadata
{
    std::optional<std::string> fmiVersion;
    std::optional<std::string> modelName;
    std::optional<std::string> guid; // Unified guid (FMI2) / instantiationToken (FMI3)
    std::optional<std::string> modelVersion;
    std::optional<std::string> author;
    std::optional<std::string> copyright;
    std::optional<std::string> license;
    std::optional<std::string> generationTool;
    std::optional<std::string> generationDateAndTime;
    std::string variableNamingConvention = "flat";
    std::optional<uint32_t> numberOfEventIndicators;
};

class ModelDescriptionCheckerBase : public Checker
{
  public:
    void validate(const std::filesystem::path& path, Certificate& cert) override;

  protected:
    // Each derived class implements version-specific validation
    virtual void performVersionSpecificChecks(xmlDocPtr doc, const std::vector<Variable>& variables,
                                              const std::map<std::string, TypeDefinition>& type_definitions,
                                              const std::map<std::string, UnitDefinition>& units,
                                              Certificate& cert) = 0;

    std::filesystem::path _fmu_root_path;

    virtual std::string getFmiVersion() const = 0;

    // Common validation methods that work the same way across FMI versions
    void checkUniqueVariableNames(const std::vector<Variable>& variables, Certificate& cert);
    void checkTypeNameClashes(const std::vector<Variable>& variables,
                              const std::map<std::string, TypeDefinition>& type_definitions, Certificate& cert);
    virtual void checkUnits(xmlDocPtr doc, Certificate& cert) = 0;
    virtual void checkTypeDefinitions(xmlDocPtr doc, Certificate& cert) = 0;
    void checkVariableNamingConvention(const std::vector<Variable>& variables, const std::string& convention,
                                       Certificate& cert);
    void checkGenerationDateAndTime(const std::optional<std::string>& generation_date_time, Certificate& cert);
    void checkFmiVersion(const std::optional<std::string>& fmi_version, Certificate& cert);
    void checkModelName(const std::optional<std::string>& model_name, Certificate& cert);
    virtual void checkGuid(const std::optional<std::string>& guid, Certificate& cert) = 0;
    void checkModelVersion(const std::optional<std::string>& version, Certificate& cert);
    void checkCopyright(const std::optional<std::string>& copyright, Certificate& cert);
    void checkLicense(const std::optional<std::string>& license, Certificate& cert);
    void checkAuthor(const std::optional<std::string>& author, Certificate& cert);
    void checkGenerationTool(const std::optional<std::string>& tool, Certificate& cert);
    void checkLogCategories(xmlDocPtr doc, Certificate& cert);
    virtual void checkAnnotations(xmlDocPtr doc, Certificate& cert) = 0;
    void checkNumberOfImplementedInterfaces(const std::map<std::string, std::string>& model_identifiers,
                                            Certificate& cert);
    void checkModelIdentifier(const std::string& model_identifier, const std::string& interface_name,
                              Certificate& cert);
    void checkDefaultExperiment(xmlDocPtr doc, Certificate& cert);

    // Common reference checks
    void checkTypeAndUnitReferences(const std::vector<Variable>& variables,
                                    const std::map<std::string, TypeDefinition>& type_definitions,
                                    const std::map<std::string, UnitDefinition>& units, Certificate& cert);
    void checkUnusedDefinitions(const std::map<std::string, TypeDefinition>& type_definitions,
                                const std::map<std::string, UnitDefinition>& units, Certificate& cert);

    // New: Check that derivative references point to valid variables
    void checkDerivativeReferences(const std::vector<Variable>& variables, Certificate& cert);

    // Version-specific validation methods (must be implemented by derived classes)
    virtual void applyDefaultInitialValues(std::vector<Variable>& variables) = 0;
    virtual void checkCausalityVariabilityInitialCombinations(const std::vector<Variable>& variables,
                                                              Certificate& cert) = 0;
    virtual void checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert) = 0;
    virtual void checkRequiredStartValues(const std::vector<Variable>& variables, Certificate& cert) = 0;
    virtual void checkIllegalStartValues(const std::vector<Variable>& variables, Certificate& cert) = 0;
    virtual void checkMinMaxStartValues(const std::vector<Variable>& variables,
                                        const std::map<std::string, TypeDefinition>& type_definitions,
                                        Certificate& cert) = 0;

    // XML parsing helpers
    virtual ModelMetadata extractMetadata(xmlNodePtr root) = 0;
    std::map<std::string, std::string> extractModelIdentifiers(xmlDocPtr doc,
                                                               const std::vector<std::string>& interface_elements);
    virtual std::map<std::string, UnitDefinition> extractUnitDefinitions(xmlDocPtr doc) = 0;
    virtual std::map<std::string, TypeDefinition> extractTypeDefinitions(xmlDocPtr doc) = 0;
    virtual std::vector<Variable> extractVariables(xmlDocPtr doc) = 0;
    std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);
    xmlXPathObjectPtr getXPathNodes(xmlDocPtr doc, const std::string& xpath);

    // Helper to check for special float values (NaN, INF)
    bool isSpecialFloat(const std::string& value);
    std::string getSpecialFloatDescription(const std::string& value);

    // Version-specific special float validation hooks
    virtual void validateVariableSpecialFloat(TestResult& test, const Variable& var, const std::string& val,
                                              const std::string& attr_name) = 0;
    virtual void validateDefaultExperimentSpecialFloat(TestResult& test, const std::string& val,
                                                       const std::string& attr_name) = 0;
    virtual void validateUnitSpecialFloat(TestResult& test, const std::string& val, const std::string& attr_name,
                                          const std::string& unit_name, size_t line) = 0;
    virtual void validateTypeDefinitionSpecialFloat(TestResult& test, const TypeDefinition& type_def,
                                                    const std::string& val, const std::string& attr_name) = 0;

    // Helper to get effective min/max for a variable considering type definitions
    struct EffectiveBounds
    {
        std::optional<std::string> min;
        std::optional<std::string> max;
    };
    EffectiveBounds getEffectiveBounds(const Variable& var,
                                       const std::map<std::string, TypeDefinition>& type_definitions);

    // Validation helpers
    template <typename T>
    bool validateTypeBounds(const Variable& var, const std::optional<std::string>& effective_min,
                            const std::optional<std::string>& effective_max, TestResult& test);

  private:
    std::set<std::string> _used_type_definitions;
    std::set<std::string> _used_units;
};

template <typename T>
bool ModelDescriptionCheckerBase::validateTypeBounds(const Variable& var,
                                                     const std::optional<std::string>& effective_min,
                                                     const std::optional<std::string>& effective_max, TestResult& test)
{
    auto parse = [&](const std::optional<std::string>& str_opt, const std::string& attr_name) -> std::optional<T>
    {
        if (!str_opt)
            return std::nullopt;

        // Check for special floats (NaN, INF) using version-specific hook
        if (std::is_floating_point_v<T> && isSpecialFloat(*str_opt))
            validateVariableSpecialFloat(test, var, *str_opt, attr_name);

        try
        {
            if constexpr (std::is_floating_point_v<T>)
                return static_cast<T>(std::stod(*str_opt));
            else if constexpr (std::is_signed_v<T>)
                return static_cast<T>(std::stoll(*str_opt));
            else
                return static_cast<T>(std::stoull(*str_opt));
        }
        catch (...)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    "): Failed to parse numeric value of " + attr_name + " with value '" + *str_opt +
                                    "'");
            return std::nullopt;
        }
    };

    // Parse the effective min/max (which may come from type definition)
    auto min_val = parse(effective_min, "min");
    auto max_val = parse(effective_max, "max");
    auto start_val = parse(var.start, "start");

    bool success = true;

    // 1. Check: max >= min
    if (min_val && max_val && *max_val < *min_val)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) + "): max (" +
                                *effective_max + ") must be >= min (" + *effective_min + ").");
        success = false;
    }

    // 2. Check: start >= min
    if (start_val && min_val && *start_val < *min_val)
    {
        test.status = TestStatus::FAIL;
        std::string msg = "Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) + "): start (" +
                          *var.start + ") must be >= min (" + *effective_min + ")";
        if (!var.min && var.declared_type)
            msg += " (min inherited from type '" + *var.declared_type + "')";
        msg += ".";
        test.messages.push_back(msg);
        success = false;
    }

    // 3. Check: start <= max
    if (start_val && max_val && *start_val > *max_val)
    {
        test.status = TestStatus::FAIL;
        std::string msg = "Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) + "): start (" +
                          *var.start + ") must be <= max (" + *effective_max + ")";
        if (!var.max && var.declared_type)
            msg += " (max inherited from type '" + *var.declared_type + "')";
        msg += ".";
        test.messages.push_back(msg);
        success = false;
    }

    return success;
}
