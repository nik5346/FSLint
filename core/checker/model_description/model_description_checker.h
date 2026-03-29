#pragma once

#include "checker.h"

#include "certificate.h"

#include <libxml/tree.h>
#include <libxml/xpath.h>

#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

/// @brief Dimension information for array variables.
struct Dimension
{
    std::optional<uint64_t> start;           ///< Fixed dimension size.
    std::optional<uint32_t> value_reference; ///< Reference to structural parameter.
    size_t sourceline = 0;                   ///< XML line number.
};

/// @brief Variable metadata from modelDescription.xml.
struct Variable
{
    std::string name;                            ///< Variable name.
    std::string type;                            ///< Variable type.
    std::string causality;                       ///< Causality.
    std::string variability;                     ///< Variability.
    std::string initial;                         ///< Initial.
    std::optional<bool> fixed;                   ///< Fixed attribute.
    std::optional<std::string> start;            ///< Start value.
    size_t num_start_values = 0;                 ///< Number of start values.
    std::optional<std::string> min;              ///< Min value.
    std::optional<std::string> max;              ///< Max value.
    std::optional<std::string> nominal;          ///< Nominal value.
    std::optional<std::string> unit;             ///< Unit.
    std::optional<std::string> display_unit;     ///< Display unit.
    std::optional<std::string> declared_type;    ///< Declared type name.
    std::optional<uint32_t> value_reference;     ///< Value reference.
    std::optional<uint32_t> derivative_of;       ///< Index of state this is derivative of.
    std::optional<bool> reinit;                  ///< Reinit attribute.
    std::optional<bool> can_handle_multiple_set; ///< CanHandleMultipleSet attribute.
    uint32_t index = 0;                          ///< 1-based index.
    std::optional<std::string> alias;            ///< Alias (FMI 1.0).
    bool is_alias = false;                       ///< True if alias.
    bool has_dimension = false;                  ///< True if array.
    std::vector<Dimension> dimensions;           ///< Dimensions.
    std::optional<std::string> clocks;           ///< Clock references.
    bool relative_quantity = false;              ///< RelativeQuantity attribute.
    size_t sourceline = 0;                       ///< XML line number.
};

/// @brief Display unit metadata.
struct DisplayUnit
{
    std::string name;                  ///< Unit name.
    std::optional<std::string> factor; ///< Factor.
    std::optional<std::string> offset; ///< Offset.
    size_t sourceline = 0;             ///< XML line number.
};

/// @brief Unit definition metadata.
struct UnitDefinition
{
    std::string name;                                 ///< Unit name.
    std::optional<std::string> factor;                ///< Factor.
    std::optional<std::string> offset;                ///< Offset.
    std::map<std::string, DisplayUnit> display_units; ///< Display units.
    size_t sourceline = 0;                            ///< XML line number.
};

/// @brief Type definition metadata.
struct TypeDefinition
{
    std::string name;                        ///< Type name.
    std::string type;                        ///< Base type.
    std::optional<std::string> min;          ///< Min.
    std::optional<std::string> max;          ///< Max.
    std::optional<std::string> nominal;      ///< Nominal.
    std::optional<std::string> unit;         ///< Unit.
    std::optional<std::string> display_unit; ///< Display unit.
    bool relative_quantity = false;          ///< RelativeQuantity.
    size_t sourceline = 0;                   ///< XML line number.
};

/// @brief Metadata from modelDescription.xml root.
struct ModelMetadata
{
    std::optional<std::string> fmiVersion;            ///< FMI version.
    std::optional<std::string> modelName;             ///< Model name.
    std::optional<std::string> guid;                  ///< GUID.
    std::optional<std::string> modelVersion;          ///< Model version.
    std::optional<std::string> author;                ///< Author.
    std::optional<std::string> copyright;             ///< Copyright.
    std::optional<std::string> license;               ///< License.
    std::optional<std::string> description;           ///< Description.
    std::optional<std::string> generationTool;        ///< Generation tool.
    std::optional<std::string> generationDateAndTime; ///< Generation timestamp.
    std::string variableNamingConvention = "flat";    ///< Naming convention.
    std::optional<uint32_t> numberOfEventIndicators;  ///< Number of event indicators.
};

/// @brief Base class for semantic model description validation.
class ModelDescriptionCheckerBase : public Checker
{
  public:
    /// @brief Validates modelDescription.xml.
    /// @param path FMU root directory.
    /// @param cert Certificate to record results.
    void validate(const std::filesystem::path& path, Certificate& cert) const override;

    /// @brief Parses a number from a string.
    /// @tparam T Numeric type.
    /// @param s String to parse.
    /// @return Parsed value or std::nullopt.
    template <typename T>
    static std::optional<T> parseNumber(std::string_view s)
    {
        // Trim whitespace
        const size_t first = s.find_first_not_of(" \t\n\r");
        if (first == std::string_view::npos)
            return std::nullopt;
        s.remove_prefix(first);

        const size_t last = s.find_last_not_of(" \t\n\r");
        if (last != std::string_view::npos)
            s.remove_suffix(s.size() - last - 1);

        if (s.empty())
            return std::nullopt;

        // std::from_chars does not support leading '+' sign
        if (s[0] == '+')
        {
            s.remove_prefix(1);
            if (s.empty())
                return std::nullopt;
        }

        if constexpr (std::is_floating_point_v<T>)
        {
            // Handle special values manually for robustness
            std::string lower;
            lower.reserve(s.size());
            for (const char c : s)
                lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            if (lower == "nan")
                return std::numeric_limits<T>::quiet_NaN();
            if (lower == "inf" || lower == "infinity")
                return std::numeric_limits<T>::infinity();
            if (lower == "-inf" || lower == "-infinity")
                return -std::numeric_limits<T>::infinity();

#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
            // Fallback for platforms without full std::from_chars support for floats
            char* endptr = nullptr;
            std::string s_str(s);
            T val = static_cast<T>(std::strtod(s_str.c_str(), &endptr));
            if (endptr == s_str.c_str() + s_str.size())
                return val;
            return std::nullopt;
#else
            T val;
            const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
            if (ec == std::errc() && ptr == s.data() + s.size())
                return val;
            return std::nullopt;
#endif
        }
        else
        {
            T val;
            const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
            if (ec == std::errc() && ptr == s.data() + s.size())
                return val;
            return std::nullopt;
        }
    }

  protected:
    /// @brief Hook for version-specific semantic checks.
    /// @param doc XML document.
    /// @param variables Extracted variables.
    /// @param type_definitions Map of type definitions.
    /// @param units Map of unit definitions.
    /// @param cert Certificate to record results.
    virtual void performVersionSpecificChecks(xmlDocPtr doc, const std::vector<Variable>& variables,
                                              const std::map<std::string, TypeDefinition>& type_definitions,
                                              const std::map<std::string, UnitDefinition>& units,
                                              Certificate& cert) const = 0;

  protected:
    /// @brief Gets FMU root path.
    /// @return Path.
    const std::filesystem::path& getFmuRootPath() const
    {
        return _fmu_root_path;
    }

    /// @brief Gets FMI version string.
    /// @return Version.
    virtual std::string getFmiVersion() const = 0;

    /// @brief Checks for duplicate variable names.
    /// @param variables List of variables.
    /// @param cert Certificate to record results.
    void checkUniqueVariableNames(const std::vector<Variable>& variables, Certificate& cert) const;

    /// @brief Checks unit definitions.
    /// @param doc XML document.
    /// @param cert Certificate to record results.
    virtual void checkUnits(xmlDocPtr doc, Certificate& cert) const = 0;

    /// @brief Checks type definitions.
    /// @param doc XML document.
    /// @param cert Certificate to record results.
    virtual void checkTypeDefinitions(xmlDocPtr doc, Certificate& cert) const = 0;

    /// @brief Checks variable names against naming convention.
    /// @param variables List of variables.
    /// @param convention Convention name.
    /// @param cert Certificate to record results.
    void checkVariableNamingConvention(const std::vector<Variable>& variables, const std::string& convention,
                                       Certificate& cert) const;

    /// @brief Checks generation date formatting.
    /// @param generation_date_time Date string.
    /// @param cert Certificate to record results.
    void checkGenerationDateAndTime(const std::optional<std::string>& generation_date_time, Certificate& cert) const;

    /// @brief Checks fmiVersion attribute presence.
    /// @param fmi_version Version string.
    /// @param cert Certificate to record results.
    virtual void checkFmiVersion(const std::optional<std::string>& fmi_version, Certificate& cert) const;

    /// @brief Validates exact value of FMI version.
    /// @param version Version string.
    /// @param test Result to update.
    virtual void validateFmiVersionValue(const std::string& version, TestResult& test) const = 0;

    /// @brief Checks modelName attribute.
    /// @param model_name Name.
    /// @param cert Certificate to record results.
    void checkModelName(const std::optional<std::string>& model_name, Certificate& cert) const;

    /// @brief Checks GUID attribute.
    /// @param guid GUID.
    /// @param cert Certificate to record results.
    virtual void checkGuid(const std::optional<std::string>& guid, Certificate& cert) const = 0;

    /// @brief Checks modelVersion attribute.
    /// @param version Version.
    /// @param cert Certificate to record results.
    void checkModelVersion(const std::optional<std::string>& version, Certificate& cert) const;

    /// @brief Checks copyright attribute.
    /// @param copyright Copyright.
    /// @param cert Certificate to record results.
    /// @param mandatory True if mandatory.
    void checkCopyright(const std::optional<std::string>& copyright, Certificate& cert, bool mandatory = true) const;

    /// @brief Checks license attribute.
    /// @param license License.
    /// @param cert Certificate to record results.
    void checkLicense(const std::optional<std::string>& license, Certificate& cert) const;

    /// @brief Checks author attribute.
    /// @param author Author.
    /// @param cert Certificate to record results.
    /// @param mandatory True if mandatory.
    void checkAuthor(const std::optional<std::string>& author, Certificate& cert, bool mandatory = true) const;

    /// @brief Checks generationTool attribute.
    /// @param tool Tool name.
    /// @param cert Certificate to record results.
    void checkGenerationTool(const std::optional<std::string>& tool, Certificate& cert) const;

    /// @brief Checks log categories.
    /// @param doc XML document.
    /// @param cert Certificate to record results.
    void checkLogCategories(xmlDocPtr doc, Certificate& cert) const;

    /// @brief Checks annotations.
    /// @param doc XML document.
    /// @param cert Certificate to record results.
    virtual void checkAnnotations(xmlDocPtr doc, Certificate& cert) const = 0;

    /// @brief Checks generation year against release year.
    /// @param dt Date string.
    /// @param generation_time Parsed time.
    /// @param test Result to update.
    virtual void checkGenerationDateReleaseYear(const std::string& dt, std::time_t generation_time,
                                                TestResult& test) const = 0;

    /// @brief Base check for generation year.
    /// @param dt Date string.
    /// @param generation_time Parsed time.
    /// @param release_year Release year.
    /// @param fmi_version FMI version.
    /// @param test Result to update.
    void checkGenerationDateReleaseYearBase(const std::string& dt, std::time_t generation_time, int32_t release_year,
                                            const std::string& fmi_version, TestResult& test) const;

    /// @brief Checks interface count.
    /// @param model_identifiers Map of identifiers.
    /// @param cert Certificate to record results.
    void checkNumberOfImplementedInterfaces(const std::map<std::string, std::string>& model_identifiers,
                                            Certificate& cert) const;

    /// @brief Checks a single model identifier.
    /// @param model_identifier Identifier.
    /// @param interface_name Interface name.
    /// @param cert Certificate to record results.
    virtual void checkModelIdentifier(const std::string& model_identifier, const std::string& interface_name,
                                      Certificate& cert) const;

    /// @brief Checks DefaultExperiment element.
    /// @param doc XML document.
    /// @param cert Certificate to record results.
    void checkDefaultExperiment(xmlDocPtr doc, Certificate& cert) const;

    /// @brief Checks type and unit references.
    /// @param variables Variables.
    /// @param type_definitions Types.
    /// @param units Units.
    /// @param cert Certificate to record results.
    void checkTypeAndUnitReferences(const std::vector<Variable>& variables,
                                    const std::map<std::string, TypeDefinition>& type_definitions,
                                    const std::map<std::string, UnitDefinition>& units, Certificate& cert) const;

    /// @brief Checks for unused unit or type definitions.
    /// @param type_definitions Types.
    /// @param units Units.
    /// @param cert Certificate to record results.
    void checkUnusedDefinitions(const std::map<std::string, TypeDefinition>& type_definitions,
                                const std::map<std::string, UnitDefinition>& units, Certificate& cert) const;

    /// @brief Propagates initial values from types to variables.
    /// @param variables List of variables.
    virtual void applyDefaultInitialValues(std::vector<Variable>& variables) const = 0;

    /// @brief Checks attribute combinations for variables.
    /// @param variables Variables.
    /// @param cert Certificate to record results.
    virtual void checkCausalityVariabilityInitialCombinations(const std::vector<Variable>& variables,
                                                              Certificate& cert) const = 0;

    /// @brief Checks legal variability.
    /// @param variables Variables.
    /// @param cert Certificate to record results.
    virtual void checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert) const = 0;

    /// @brief Checks required start values.
    /// @param variables Variables.
    /// @param cert Certificate to record results.
    virtual void checkRequiredStartValues(const std::vector<Variable>& variables, Certificate& cert) const = 0;

    /// @brief Checks prohibited start values.
    /// @param variables Variables.
    /// @param cert Certificate to record results.
    virtual void checkIllegalStartValues(const std::vector<Variable>& variables, Certificate& cert) const = 0;

    /// @brief Checks start values against min/max bounds.
    /// @param variables Variables.
    /// @param type_definitions Types.
    /// @param cert Certificate to record results.
    virtual void checkMinMaxStartValues(const std::vector<Variable>& variables,
                                        const std::map<std::string, TypeDefinition>& type_definitions,
                                        Certificate& cert) const = 0;

    /// @brief Extracts metadata from XML root.
    /// @param root XML node.
    /// @return Metadata.
    virtual ModelMetadata extractMetadata(xmlNodePtr root) const = 0;

    /// @brief Extracts model identifiers.
    /// @param doc XML document.
    /// @param interface_elements List of elements.
    /// @return Map of identifiers.
    virtual std::map<std::string, std::string>
    extractModelIdentifiers(xmlDocPtr doc, const std::vector<std::string>& interface_elements) const;

    /// @brief Extracts unit definitions.
    /// @param doc XML document.
    /// @return Map of units.
    virtual std::map<std::string, UnitDefinition> extractUnitDefinitions(xmlDocPtr doc) const = 0;

    /// @brief Extracts type definitions.
    /// @param doc XML document.
    /// @return Map of types.
    virtual std::map<std::string, TypeDefinition> extractTypeDefinitions(xmlDocPtr doc) const = 0;

    /// @brief Extracts variables.
    /// @param doc XML document.
    /// @return List of variables.
    virtual std::vector<Variable> extractVariables(xmlDocPtr doc) const = 0;

    /// @brief Gets XML attribute value.
    /// @param node XML node.
    /// @param attr_name Name.
    /// @return Value or std::nullopt.
    std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name) const;

    /// @brief Gets XPath nodes from document.
    /// @param doc XML document.
    /// @param xpath XPath string.
    /// @return XPath object pointer.
    xmlXPathObjectPtr getXPathNodes(xmlDocPtr doc, const std::string& xpath) const;

    /// @brief Checks if value is NaN or INF.
    /// @param value String value.
    /// @return True if special.
    bool isSpecialFloat(const std::string& value) const;

    /// @brief Gets description of special float.
    /// @param value String value.
    /// @return Description.
    std::string getSpecialFloatDescription(const std::string& value) const;

    /// @brief Normalizes float string.
    /// @param value String value.
    /// @return Normalized string.
    std::string normalizeFloatString(const std::string& value) const;

    /// @brief Hook to validate special float in variable.
    /// @param test Result to update.
    /// @param var Variable.
    /// @param val Value.
    /// @param attr_name Attribute.
    virtual void validateVariableSpecialFloat(TestResult& test, const Variable& var, const std::string& val,
                                              const std::string& attr_name) const = 0;

    /// @brief Hook to validate special float in DefaultExperiment.
    /// @param test Result to update.
    /// @param val Value.
    /// @param attr_name Attribute.
    virtual void validateDefaultExperimentSpecialFloat(TestResult& test, const std::string& val,
                                                       const std::string& attr_name) const = 0;

    /// @brief Hook to validate special float in unit.
    /// @param test Result to update.
    /// @param val Value.
    /// @param attr_name Attribute.
    /// @param unit_name Unit name.
    /// @param line Line number.
    virtual void validateUnitSpecialFloat(TestResult& test, const std::string& val, const std::string& attr_name,
                                          const std::string& unit_name, size_t line) const = 0;

    /// @brief Hook to validate special float in type.
    /// @param test Result to update.
    /// @param type_def Type.
    /// @param val Value.
    /// @param attr_name Attribute.
    virtual void validateTypeDefinitionSpecialFloat(TestResult& test, const TypeDefinition& type_def,
                                                    const std::string& val, const std::string& attr_name) const = 0;

    /// @brief Effective bounds for a variable.
    struct EffectiveBounds
    {
        std::optional<std::string> min; ///< Min.
        std::optional<std::string> max; ///< Max.
    };

    /// @brief Gets effective bounds for variable.
    /// @param var Variable.
    /// @param type_definitions Types.
    /// @return Bounds.
    EffectiveBounds getEffectiveBounds(const Variable& var,
                                       const std::map<std::string, TypeDefinition>& type_definitions) const;

    /// @brief Validates numeric bounds for variable.
    /// @tparam T Numeric type.
    /// @param var Variable.
    /// @param effective_min Effective min.
    /// @param effective_max Effective max.
    /// @param test Result to update.
    /// @return True if valid.
    template <typename T>
    bool validateTypeBounds(const Variable& var, const std::optional<std::string>& effective_min,
                            const std::optional<std::string>& effective_max, TestResult& test) const;

  private:
    mutable std::filesystem::path _fmu_root_path;
    mutable std::set<std::string> _used_type_definitions;
    mutable std::set<std::string> _used_units;
};

template <typename T>
bool ModelDescriptionCheckerBase::validateTypeBounds(const Variable& var,
                                                     const std::optional<std::string>& effective_min,
                                                     const std::optional<std::string>& effective_max,
                                                     TestResult& test) const
{
    auto parse = [&](const std::optional<std::string>& str_opt, const std::string& attr_name) -> std::optional<T>
    {
        if (!str_opt)
            return std::nullopt;

        // Check for special floats (NaN, INF) using version-specific hook
        if constexpr (std::is_floating_point_v<T>)
        {
            if (isSpecialFloat(*str_opt))
                validateVariableSpecialFloat(test, var, *str_opt, attr_name);
        }

        const auto val = parseNumber<T>(*str_opt);
        if (!val)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    "): Failed to parse numeric value of " + attr_name + " with value '" + *str_opt +
                                    "'");
            return std::nullopt;
        }
        return val;
    };

    // Parse the effective min/max (which may come from type definition)
    auto min_val = parse(effective_min, "min");
    auto max_val = parse(effective_max, "max");
    auto start_val = parse(var.start, "start");

    bool success = true;

    // 1. Check: max >= min
    if (min_val.has_value() && max_val.has_value() && max_val.value() < min_val.value())
    {
        test.status = TestStatus::FAIL;
        std::string msg = "Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) + "): max (";
        if (effective_max.has_value())
            msg += effective_max.value();
        msg += ") must be >= min (";
        if (effective_min.has_value())
            msg += effective_min.value();
        msg += ").";
        test.messages.push_back(msg);
        success = false;
    }

    // 2. Check: start >= min
    if (start_val.has_value() && min_val.has_value() && start_val.value() < min_val.value())
    {
        test.status = TestStatus::FAIL;
        std::string msg = "Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) + "): start (";
        if (var.start.has_value())
            msg += var.start.value();
        msg += ") must be >= min (";
        if (effective_min.has_value())
            msg += effective_min.value();
        msg += ")";
        if (!var.min && var.declared_type)
            msg += " (min inherited from type '" + *var.declared_type + "')";
        msg += ".";
        test.messages.push_back(msg);
        success = false;
    }

    // 3. Check: start <= max
    if (start_val.has_value() && max_val.has_value() && start_val.value() > max_val.value())
    {
        test.status = TestStatus::FAIL;
        std::string msg = "Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) + "): start (";
        if (var.start.has_value())
            msg += var.start.value();
        msg += ") must be <= max (";
        if (effective_max.has_value())
            msg += effective_max.value();
        msg += ")";
        if (!var.max && var.declared_type)
            msg += " (max inherited from type '" + *var.declared_type + "')";
        msg += ".";
        test.messages.push_back(msg);
        success = false;
    }

    return success;
}
