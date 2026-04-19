#include "fmi3_model_description_checker.h"

#include "model_description_checker.h"

#include "certificate.h"

#include <libxml/parser.h>
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <format>
#include <map>
#include <optional>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

void Fmi3ModelDescriptionChecker::performVersionSpecificChecks(
    xmlDocPtr doc, const std::vector<Variable>& variables,
    const std::map<std::string, TypeDefinition>& type_definitions,
    [[maybe_unused]] const std::map<std::string, UnitDefinition>& units, Certificate& cert) const
{
    // FMI3-specific checks
    checkEnumerationVariables(variables, cert);
    checkIndependentVariable(variables, cert);
    checkDimensionReferences(variables, cert);
    checkArrayStartValues(variables, cert);
    checkClockReferences(variables, cert);
    checkClockedVariables(variables, cert);
    checkAliases(variables, type_definitions, cert);
    checkAliasElements(doc, variables, units, cert);
    checkReinitAttribute(doc, variables, cert);
    checkDerivativeConsistency(doc, variables, cert);
    checkCanHandleMultipleSet(variables, cert);
    checkClockTypes(doc, cert);
    checkStructuralParameter(variables, cert);
    checkCapabilityFlags(doc, cert);
    checkClockAttributeConsistency(doc, variables, cert);
    checkDerivativeDimensions(variables, cert);
    checkModelStructure(doc, variables, cert);
}

std::vector<Variable> Fmi3ModelDescriptionChecker::extractVariables(xmlDocPtr doc) const
{
    std::vector<Variable> variables;

    // FMI3: Direct type elements under ModelVariables
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelVariables/*");
    if (xpath_obj == nullptr)
        return variables;

    xmlNodeSetPtr nodes = xpath_obj->nodesetval;
    if (nodes == nullptr)
    {
        xmlXPathFreeObject(xpath_obj);
        return variables;
    }

    for (int32_t i = 0; i < nodes->nodeNr; ++i)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr node = nodes->nodeTab[i];
        Variable var;
        var.index = static_cast<uint32_t>(i + 1);

        var.name = getXmlAttribute(node, "name").value_or("");
        var.type = getVariableType(node);
        var.causality = getXmlAttribute(node, "causality").value_or("local");
        var.variability = getXmlAttribute(node, "variability").value_or("");

        // Apply default variability (FMI3-specific rules)
        if (var.variability.empty() && (var.type == "Float32" || var.type == "Float64") &&
            !(var.causality == "parameter" || var.causality == "structuralParameter" ||
              var.causality == "calculatedParameter"))
            var.variability = "continuous";
        else if (var.variability.empty() &&
                 (var.causality == "input" || var.causality == "output" || var.causality == "local") &&
                 !(var.type == "Float32" || var.type == "Float64"))
            var.variability = "discrete";

        var.initial = getXmlAttribute(node, "initial").value_or("");
        var.start = getXmlAttribute(node, "start");
        if (var.start.has_value())
        {
            std::string s = *var.start;
            std::ranges::replace(s, ',', ' ');
            std::stringstream ss(s);
            std::string token;
            while (ss >> token)
                if (!token.empty())
                    var.num_start_values++;
        }

        var.min = getXmlAttribute(node, "min");
        var.max = getXmlAttribute(node, "max");
        var.nominal = getXmlAttribute(node, "nominal");

        // FMI3: Check for Start element child for String/Binary types
        if (var.type == "String" || var.type == "Binary")
        {
            for (xmlNodePtr child = node->children; child != nullptr; child = child->next)
            {
                if (child->type == XML_ELEMENT_NODE &&
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                    xmlStrcmp(child->name, reinterpret_cast<const xmlChar*>("Start")) == 0)
                {
                    if (!var.start.has_value())
                        var.start = getXmlAttribute(child, "value");
                    var.num_start_values++;
                }
            }
        }

        var.unit = getXmlAttribute(node, "unit");
        var.display_unit = getXmlAttribute(node, "displayUnit");
        var.declared_type = getXmlAttribute(node, "declaredType");
        var.clocks = getXmlAttribute(node, "clocks");

        if (auto p = getXmlAttribute(node, "previous"))
            var.previous = parseNumber<uint32_t>(*p);

        extractDimensions(node, var);
        var.sourceline = node->line;

        static auto parse_bool = [](const std::optional<std::string>& s) -> std::optional<bool>
        {
            if (!s)
                return std::nullopt;
            std::string val = *s;
            val.erase(std::ranges::remove_if(val, ::isspace).begin(), val.end());
            if (val == "true" || val == "1")
                return true;
            if (val == "false" || val == "0")
                return false;
            return std::nullopt;
        };

        auto vr = getXmlAttribute(node, "valueReference");
        if (vr.has_value())
            var.value_reference = parseNumber<uint32_t>(*vr);

        // FMI3: derivative attribute is on the variable element itself
        auto der = getXmlAttribute(node, "derivative");
        if (der.has_value())
            var.derivative_of = parseNumber<uint32_t>(*der);

        var.reinit = parse_bool(getXmlAttribute(node, "reinit"));
        var.can_handle_multiple_set = parse_bool(getXmlAttribute(node, "canHandleMultipleSetPerTimeInstant"));

        variables.push_back(var);
    }

    xmlXPathFreeObject(xpath_obj);
    return variables;
}

std::string Fmi3ModelDescriptionChecker::getVariableType(xmlNodePtr node) const
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<const char*>(node->name);
}

void Fmi3ModelDescriptionChecker::applyDefaultInitialValues(std::vector<Variable>& variables) const
{
    for (auto& var : variables)
    {
        if (!var.initial.empty())
            continue;

        // FMI3: initial NOT ALLOWED for independent - keep empty
        if (var.causality == "independent")
        {
            var.initial = "";
            continue;
        }

        // Apply defaults based on FMI3 spec
        if (var.causality == "structuralParameter" || var.causality == "parameter")
        {
            if (var.variability == "fixed" || var.variability == "tunable")
                var.initial = "exact";
        }
        else if (var.causality == "calculatedParameter")
        {
            if (var.variability == "fixed" || var.variability == "tunable")
                var.initial = "calculated";
        }
        else if (var.causality == "input")
        {
            if (var.variability == "discrete" || var.variability == "continuous")
                var.initial = "exact";
        }
        else if (var.causality == "output")
        {
            if (var.variability == "constant")
                var.initial = "exact";
            else if (var.variability == "discrete" || var.variability == "continuous")
                var.initial = "calculated";
        }
        else if (var.causality == "local")
        {
            if (var.variability == "constant")
                var.initial = "exact";
            else if (var.variability == "fixed" || var.variability == "tunable" || var.variability == "discrete" ||
                     var.variability == "continuous")
                var.initial = "calculated";
        }
    }
}

void Fmi3ModelDescriptionChecker::checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert) const
{
    TestResult test{"Legal Variability", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        // FMI3: Non-Real types (Float32, Float64) cannot be continuous
        if (var.type != "Float32" && var.type != "Float64" && var.variability == "continuous")
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Variable '{}' (line {}) is of type {} and cannot have variability 'continuous'. Only "
                            "variables of type Float32 or Float64 can be continuous.",
                            var.name, var.sourceline, var.type));
        }

        // FMI3: causality="parameter", "calculatedParameter" or "structuralParameter" must have variability="fixed" or
        // "tunable"
        if ((var.causality == "parameter" || var.causality == "calculatedParameter" ||
             var.causality == "structuralParameter") &&
            (var.variability != "fixed" && var.variability != "tunable"))
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format("Variable '{}' (line {}) has causality='{}' but "
                                                        "variability='{}'. Parameters must be 'fixed' or 'tunable'.",
                                                        var.name, var.sourceline, var.causality, var.variability));
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkClockTypes(xmlDocPtr doc, Certificate& cert) const
{
    TestResult test{"Clock Type Validation", TestStatus::PASS, {}};

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//TypeDefinitions/ClockType");
    if (xpath_obj == nullptr || xpath_obj->nodesetval == nullptr)
    {
        if (xpath_obj != nullptr)
            xmlXPathFreeObject(xpath_obj);
        cert.printTestResult(test);
        return;
    }

    for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
        auto name = getXmlAttribute(node, "name").value_or("unnamed");
        auto iv = getXmlAttribute(node, "intervalVariability").value_or("");

        // intervalDecimal required if Clock type is constant, fixed or tunable periodic Clocks.
        if (iv == "constant" || iv == "fixed" || iv == "tunable")
        {
            auto id = getXmlAttribute(node, "intervalDecimal");
            auto ic = getXmlAttribute(node, "intervalCounter");
            if (!id && !ic)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("ClockType '{}' (line {}) has intervalVariability='{}' but missing 'intervalDecimal' "
                                "or 'intervalCounter'.",
                                name, node->line, iv));
            }
        }
    }

    xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkRequiredStartValues(const std::vector<Variable>& variables,
                                                           Certificate& cert) const
{
    TestResult test{"Required Start Values", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        // Skip Clock types
        if (var.type == "Clock")
            continue;

        bool needs_start = false;

        // Rule 1: initial = "exact" or "approx" requires start
        if (var.initial == "exact" || var.initial == "approx")
            needs_start = true;

        // Rule 2: causality = "parameter", "structuralParameter", or "input" requires start
        if (var.causality == "parameter" || var.causality == "structuralParameter" || var.causality == "input")
            needs_start = true;

        // Rule 3: variability = "constant" requires start
        if (var.variability == "constant")
            needs_start = true;

        if (needs_start && !var.start.has_value())
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Variable '{}' (line {}) must have a start value.", var.name, var.sourceline));
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkCausalityVariabilityInitialCombinations(const std::vector<Variable>& variables,
                                                                               Certificate& cert) const
{
    TestResult test{"Causality/Variability/Initial Combinations", TestStatus::PASS, {}};

    // Legal combinations for FMI 3.0
    const std::set<std::tuple<std::string, std::string, std::string>> legal_combinations = {
        // FMI3: structuralParameter
        {"structuralParameter", "fixed", "exact"},
        {"structuralParameter", "tunable", "exact"},

        {"parameter", "fixed", "exact"},
        {"parameter", "tunable", "exact"},

        {"calculatedParameter", "fixed", "calculated"},
        {"calculatedParameter", "fixed", "approx"},
        {"calculatedParameter", "tunable", "calculated"},
        {"calculatedParameter", "tunable", "approx"},

        {"input", "discrete", "exact"},
        {"input", "continuous", "exact"},

        {"output", "constant", "exact"},
        {"output", "discrete", "calculated"},
        {"output", "discrete", "exact"},
        {"output", "discrete", "approx"},
        {"output", "continuous", "calculated"},
        {"output", "continuous", "exact"},
        {"output", "continuous", "approx"},

        {"local", "constant", "exact"},
        {"local", "fixed", "calculated"},
        {"local", "fixed", "approx"},
        {"local", "tunable", "calculated"},
        {"local", "tunable", "approx"},
        {"local", "discrete", "calculated"},
        {"local", "discrete", "exact"},
        {"local", "discrete", "approx"},
        {"local", "continuous", "calculated"},
        {"local", "continuous", "exact"},
        {"local", "continuous", "approx"},

        // FMI3: independent must NOT have initial attribute
        {"independent", "continuous", ""},
    };

    for (const auto& var : variables)
    {
        const std::string initial = var.initial.empty() ? "" : var.initial;
        auto combination = std::make_tuple(var.causality, var.variability, initial);

        if (!legal_combinations.contains(combination))
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format(
                "Variable '{}' (line {}) has illegal combination: causality='{}', variability='{}', initial='{}'",
                var.name, var.sourceline, var.causality, var.variability, initial));
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkIllegalStartValues(const std::vector<Variable>& variables,
                                                          Certificate& cert) const
{
    TestResult test{"Illegal Start Values", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        // Variables with initial="calculated" should not have start values
        if (var.initial == "calculated" && var.start.has_value())
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Variable '{}' (line {}) has initial='calculated' but provides a start value.", var.name,
                            var.sourceline));
        }

        // FMI3: Independent variables should not have start values
        if (var.causality == "independent" && var.start.has_value())
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Variable '{}' (line {}) has causality='independent' but provides a start value.", var.name,
                            var.sourceline));
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkMinMaxStartValues(const std::vector<Variable>& variables,
                                                         const std::map<std::string, TypeDefinition>& type_definitions,
                                                         Certificate& cert) const
{
    TestResult test{"Min/Max/Start Value Constraints", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        // Skip non-numeric types
        if (var.type != "Enumeration" && var.type != "Float32" && var.type != "Float64" && var.type != "Int8" &&
            var.type != "UInt8" && var.type != "Int16" && var.type != "UInt16" && var.type != "Int32" &&
            var.type != "UInt32" && var.type != "Int64" && var.type != "UInt64")
        {
            continue;
        }

        // Get effective bounds (considering type definitions)
        const EffectiveBounds bounds = getEffectiveBounds(var, type_definitions);

        // Validate variable's bounds using the appropriate type
        if (var.type == "Float32")
            validateTypeBounds<float>(var, bounds.min, bounds.max, test);
        else if (var.type == "Float64")
            validateTypeBounds<double>(var, bounds.min, bounds.max, test);
        else if (var.type == "Enumeration" || var.type == "Int64")
            validateTypeBounds<int64_t>(var, bounds.min, bounds.max, test);
        else if (var.type == "Int8")
            validateTypeBounds<int8_t>(var, bounds.min, bounds.max, test);
        else if (var.type == "UInt8")
            validateTypeBounds<uint8_t>(var, bounds.min, bounds.max, test);
        else if (var.type == "Int16")
            validateTypeBounds<int16_t>(var, bounds.min, bounds.max, test);
        else if (var.type == "UInt16")
            validateTypeBounds<uint16_t>(var, bounds.min, bounds.max, test);
        else if (var.type == "Int32")
            validateTypeBounds<int32_t>(var, bounds.min, bounds.max, test);
        else if (var.type == "UInt32")
            validateTypeBounds<uint32_t>(var, bounds.min, bounds.max, test);
        else if (var.type == "UInt64")
            validateTypeBounds<uint64_t>(var, bounds.min, bounds.max, test);
    }

    cert.printTestResult(test);
}
void Fmi3ModelDescriptionChecker::checkEnumerationVariables(const std::vector<Variable>& variables,
                                                            Certificate& cert) const
{
    TestResult test{"Enumeration Variable Type", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        if (var.type == "Enumeration" && !var.declared_type.has_value())
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Variable '{}' (line {}) is of type Enumeration and must have a declaredType attribute.",
                            var.name, var.sourceline));
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkIndependentVariable(const std::vector<Variable>& variables,
                                                           Certificate& cert) const
{
    TestResult test{"Independent Variable", TestStatus::PASS, {}};

    int32_t independent_count = 0;
    for (const auto& var : variables)
    {
        if (var.causality == "independent")
        {
            independent_count++;

            // FMI3: Check type (Float32, Float64)
            if (var.type != "Float32" && var.type != "Float64")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "Independent variable '{}' (line {}) must be of floating point type (Float32 or Float64).",
                    var.name, var.sourceline));
            }

            // FMI3: Check for illegal initial attribute
            if (!var.initial.empty())
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Independent variable '{}' (line {}) must not have an initial attribute.", var.name,
                                var.sourceline));
            }

            // FMI3: Check for illegal start attribute
            if (var.start.has_value())
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "Independent variable '{}' (line {}) must not have a start attribute.", var.name, var.sourceline));
            }
        }
    }

    // FMI3: Exactly one independent variable required
    if (independent_count != 1)
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back(
            std::format("Exactly one independent variable must be defined, found {}).", independent_count));
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkDerivativeConsistency(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                             Certificate& cert) const
{
    TestResult test{"Derivative Consistency", TestStatus::PASS, {}};

    std::map<uint32_t, const Variable*> vr_map;
    for (const auto& var : variables)
        if (var.value_reference.has_value())
            vr_map[*var.value_reference] = &var;

    // Parse ModelStructure/ContinuousStateDerivative to identify "active" derivatives
    std::set<uint32_t> active_derivative_vrs;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/ContinuousStateDerivative");

    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            const auto vr_str = getXmlAttribute(node, "valueReference");

            if (vr_str.has_value())
            {
                if (const auto vr_opt = parseNumber<uint32_t>(*vr_str))
                    active_derivative_vrs.insert(*vr_opt);
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    for (const auto& var : variables)
    {
        if (var.derivative_of.has_value())
        {
            // 1. Variability of derivative must be continuous
            if (var.variability != "continuous")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Variable '{}' (line {}) is a derivative and must have variability='continuous'.",
                                var.name, var.sourceline));
            }

            // 2. Must be Float32 or Float64
            if (var.type != "Float32" && var.type != "Float64")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format("Variable '{}' (line {}) has 'derivative' attribute but is "
                                                            "of type {} (must be Float32 or Float64).",
                                                            var.name, var.sourceline, var.type));
            }

            const uint32_t ref_vr = *var.derivative_of;
            auto it = vr_map.find(ref_vr);

            if (it == vr_map.end())
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format("Variable '{}' (line {}) has derivative attribute "
                                                            "referencing value reference {} which does not exist.",
                                                            var.name, var.sourceline, ref_vr));
            }
            else
            {
                // 3. State variable must have variability="continuous" - ONLY if it's an active derivative
                if (var.value_reference.has_value() && active_derivative_vrs.contains(*var.value_reference))
                {
                    const Variable* state_var = it->second;
                    if (state_var->variability != "continuous")
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(std::format(
                            "Variable '{}' (line {}) is derivative of '{}' (line {}) which has variability '{}'. "
                            "Continuous-time states must have variability='continuous'.",
                            var.name, var.sourceline, state_var->name, state_var->sourceline, state_var->variability));
                    }
                }
            }
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkCanHandleMultipleSet(const std::vector<Variable>& variables,
                                                            Certificate& cert) const
{
    TestResult test{"CanHandleMultipleSetPerTimeInstant", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        if (var.can_handle_multiple_set.has_value() && var.causality != "input")
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Variable '{}' (line {}) has 'canHandleMultipleSetPerTimeInstant' but causality is '{}' "
                            "(must be 'input').",
                            var.name, var.sourceline, var.causality));
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkReinitAttribute(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                       Certificate& cert) const
{
    TestResult test{"Reinit Attribute", TestStatus::PASS, {}};

    // Continuous-time states are variables referenced by 'derivative' attribute of some other variable
    // which is itself listed in ContinuousStateDerivative.
    std::set<uint32_t> state_vrs;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/ContinuousStateDerivative");

    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        std::map<uint32_t, const Variable*> vr_to_var;
        for (const auto& v : variables)
            if (v.value_reference.has_value())
                vr_to_var[*v.value_reference] = &v;

        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            const auto vr_str = getXmlAttribute(node, "valueReference");

            if (vr_str.has_value())
            {
                if (const auto vr_opt = parseNumber<uint32_t>(*vr_str))
                {
                    auto it = vr_to_var.find(*vr_opt);
                    if (it != vr_to_var.end())
                    {
                        const auto& var_ref = *it->second;
                        if (var_ref.derivative_of.has_value())
                            state_vrs.insert(var_ref.derivative_of.value());
                    }
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    for (const auto& var : variables)
    {
        if (var.reinit.has_value())
        {
            // FMI3: reinit may only be present for continuous-time states
            if (!var.value_reference.has_value() || !state_vrs.contains(*var.value_reference))
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Variable '{}' (line {}) has 'reinit' attribute but is not a continuous-time state.",
                                var.name, var.sourceline));
            }

            // 2. Must be Float32 or Float64 (only these can be continuous-time states anyway)
            if (var.type != "Float32" && var.type != "Float64")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format("Variable '{}' (line {}) has 'reinit' attribute but is of "
                                                            "type '{}'. It must be Float32 or Float64.",
                                                            var.name, var.sourceline, var.type));
            }
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkAliasElements(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                       const std::map<std::string, UnitDefinition>& units,
                                                       Certificate& cert) const
{
    TestResult test{"Alias Elements", TestStatus::PASS, {}};

    std::map<uint32_t, const Variable*> vr_map;
    std::map<std::string, const Variable*> name_map;
    for (const auto& var : variables)
    {
        if (var.value_reference.has_value())
            vr_map[*var.value_reference] = &var;
        name_map[var.name] = &var;
    }

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelVariables/*");
    if (xpath_obj == nullptr || xpath_obj->nodesetval == nullptr)
    {
        if (xpath_obj != nullptr)
            xmlXPathFreeObject(xpath_obj);
        cert.printTestResult(test);
        return;
    }

    // For cycle detection
    std::map<std::string, std::string> alias_to_aliased;

    for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr var_node = xpath_obj->nodesetval->nodeTab[i];
        auto var_name = getXmlAttribute(var_node, "name").value_or("unnamed");
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        std::string var_type = reinterpret_cast<const char*>(var_node->name);
        auto var_unit = getXmlAttribute(var_node, "unit");

        for (xmlNodePtr child = var_node->children; child != nullptr; child = child->next)
        {
            if (child->type == XML_ELEMENT_NODE &&
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                xmlStrcmp(child->name, reinterpret_cast<const xmlChar*>("Alias")) == 0)
            {
                auto vr_str = getXmlAttribute(child, "valueReference");
                if (!vr_str)
                    continue;

                auto vr_opt = parseNumber<uint32_t>(*vr_str);
                if (!vr_opt)
                    continue;

                uint32_t vr = *vr_opt;
                auto it = vr_map.find(vr);

                if (it == vr_map.end())
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(
                        std::format("Variable '{}' (line {}): <Alias> references non-existent valueReference {}.",
                                    var_name, child->line, vr));
                }
                else
                {
                    const Variable* aliased_var = it->second;

                    // Rule: The type of the alias variable must match the type of the aliased variable
                    if (var_type != aliased_var->type)
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(std::format(
                            "Variable '{}' (line {}): <Alias> type mismatch. Parent is {}, aliased variable '{}' is {}.",
                            var_name, child->line, var_type, aliased_var->name, aliased_var->type));
                    }

                    // For cycle detection
                    alias_to_aliased[var_name] = aliased_var->name;

                    // Rule: If displayUnit is specified on the <Alias>: that unit name must exist
                    auto displayUnit = getXmlAttribute(child, "displayUnit");
                    if (displayUnit)
                    {
                        if (!var_unit)
                        {
                            if (test.getStatus() != TestStatus::FAIL)
                                test.setStatus(TestStatus::WARNING);
                            test.getMessages().emplace_back(
                                std::format("Variable '{}' (line {}): <Alias> specifies displayUnit='{}' but parent "
                                            "variable has no unit attribute.",
                                            var_name, child->line, *displayUnit));
                        }
                        else
                        {
                            auto unit_it = units.find(*var_unit);
                            if (unit_it == units.end() || unit_it->second.display_units.find(*displayUnit) ==
                                                              unit_it->second.display_units.end())
                            {
                                test.setStatus(TestStatus::FAIL);
                                test.getMessages().emplace_back(std::format(
                                    "Variable '{}' (line {}): <Alias> specifies displayUnit '{}' which is not defined "
                                    "for unit '{}'.",
                                    var_name, child->line, *displayUnit, *var_unit));
                            }
                        }
                    }
                }
            }
        }
    }

    // Rule: Alias cycles: if variable A aliases variable B, and B also has an <Alias> pointing back to A
    for (const auto& [alias, aliased] : alias_to_aliased)
    {
        std::string current = aliased;
        std::set<std::string> visited = {alias};

        while (alias_to_aliased.contains(current))
        {
            if (visited.contains(current))
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format("Circular alias chain detected starting from '{}'.", alias));
                break;
            }
            visited.insert(current);
            current = alias_to_aliased.at(current);
        }
    }

    xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkAliases(const std::vector<Variable>& variables,
                                               const std::map<std::string, TypeDefinition>& type_definitions,
                                               Certificate& cert) const
{
    TestResult test{"Alias Variables", TestStatus::PASS, {}};

    auto resolve_unit = [&](const Variable* v) -> std::optional<std::string>
    {
        if (v->unit.has_value())
            return v->unit;
        if (v->declared_type.has_value())
        {
            auto it = type_definitions.find(*v->declared_type);
            if (it != type_definitions.end())
                return it->second.unit;
        }
        return std::nullopt;
    };

    // Group variables by valueReference
    std::map<uint32_t, std::vector<const Variable*>> vr_to_vars;
    for (const auto& var : variables)
        if (var.value_reference.has_value())
            vr_to_vars[*var.value_reference].push_back(&var);

    for (const auto& [vr, alias_set] : vr_to_vars)
    {
        if (alias_set.size() <= 1)
            continue;

        const Variable* first = alias_set[0];

        for (size_t i = 1; i < alias_set.size(); ++i)
        {
            const Variable* var = alias_set[i];

            // 1. Same base type
            if (var->type != first->type)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("All variables in an alias set (VR {}) must have the same type. Variable '{}' is {} "
                                "but '{}' is {}.",
                                vr, var->name, var->type, first->name, first->type));
            }

            // 2. Same unit and displayUnit
            const auto var_unit = resolve_unit(var);
            const auto first_unit = resolve_unit(first);
            if (var_unit != first_unit)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "All variables in an alias set (VR {}) must have the same unit. Variable '{}' has "
                    "unit '{}' but '{}' has unit '{}'.",
                    vr, var->name, var_unit.value_or("(none)"), first->name, first_unit.value_or("(none)")));
            }
            if (var->display_unit != first->display_unit)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format("All variables in an alias set (VR {}) must have the same "
                                                            "displayUnit. Variable '{}' has displayUnit '{}' but '{}' "
                                                            "has displayUnit '{}'.",
                                                            vr, var->name, var->display_unit.value_or("(none)"),
                                                            first->name, first->display_unit.value_or("(none)")));
            }

            // 3. Same relativeQuantity
            if (var->relative_quantity != first->relative_quantity)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("All variables in an alias set (VR {}) must have the same relativeQuantity attribute. "
                                "Variable '{}' differs from '{}'.",
                                vr, var->name, first->name));
            }

            // 4. Same variability
            if (var->variability != first->variability)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format("All variables in an alias set (VR {}) must have the same "
                                                            "variability. Variable '{}' is {} but '{}' is {}.",
                                                            vr, var->name, var->variability, first->name,
                                                            first->variability));
            }

            // 5. Same dimensions
            if (!compareDimensions(*var, *first))
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format("All variables in an alias set (VR {}) must have the same "
                                                            "dimensions. Variable '{}' dimensions do not match '{}'.",
                                                            vr, var->name, first->name));
            }
        }

        // 5. Causality: At most one variable in an alias set can be non-local.
        std::vector<const Variable*> non_local;
        for (const auto* v : alias_set)
            if (v->causality != "local")
                non_local.push_back(v);

        if (non_local.size() > 1)
        {
            test.setStatus(TestStatus::FAIL);
            std::string vars;
            for (size_t i = 0; i < non_local.size(); ++i)
                vars += (i > 0 ? ", " : "") + std::format("'{}'", non_local[i]->name);

            test.getMessages().emplace_back(std::format(
                "All variables in an alias set (VR {}) must have at most one variable with causality other than "
                "'local'. Found: {}.",
                vr, vars));
        }

        // 6. Constant variables: must have identical start values if they are aliased
        if (first->variability == "constant")
        {
            for (const auto* var : alias_set)
            {
                if (var->start != first->start)
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format(
                        "All variables in an alias set (VR {}) must have the same start values if they are constant. "
                        "Variable '{}' has start='{}' but '{}' has start='{}'.",
                        vr, var->name, var->start.value_or("(none)"), first->name, first->start.value_or("(none)")));
                }
            }
        }

        // 7. Start attributes: At most one non-constant variable with start
        std::vector<const Variable*> non_constant_with_start;
        for (const auto* v : alias_set)
            if (v->variability != "constant" && v->start.has_value())
                non_constant_with_start.push_back(v);

        if (non_constant_with_start.size() > 1)
        {
            test.setStatus(TestStatus::FAIL);
            std::string vars;
            for (size_t i = 0; i < non_constant_with_start.size(); ++i)
                vars += (i > 0 ? ", " : "") + std::format("'{}'", non_constant_with_start[i]->name);

            test.getMessages().emplace_back(std::format(
                "All variables in an alias set (VR {}) must have at most one non-constant variable with a start "
                "attribute. Found: {}.",
                vr, vars));
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkClockAttributeConsistency(xmlDocPtr doc, const std::vector<Variable>& /*variables*/,
                                                                   Certificate& cert) const
{
    TestResult test{"Clock Attribute Consistency", TestStatus::PASS, {}};

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelVariables/*");
    if (xpath_obj == nullptr || xpath_obj->nodesetval == nullptr)
    {
        if (xpath_obj != nullptr)
            xmlXPathFreeObject(xpath_obj);
        cert.printTestResult(test);
        return;
    }

    for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        std::string type = reinterpret_cast<const char*>(node->name);
        auto causality = getXmlAttribute(node, "causality").value_or("local");

        if (type != "Clock" && causality != "clock")
            continue;

        auto name = getXmlAttribute(node, "name").value_or("unnamed");
        auto clockType = getXmlAttribute(node, "clockType").value_or("periodic");
        auto intervalVariability = getXmlAttribute(node, "intervalVariability").value_or("");

        auto period = getXmlAttribute(node, "period");
        auto intervalCounter = getXmlAttribute(node, "intervalCounter");
        auto shiftDecimal = getXmlAttribute(node, "shiftDecimal");
        auto shiftFraction = getXmlAttribute(node, "shiftFraction");
        auto resolution = getXmlAttribute(node, "resolution");

        // Rule: If clockType="periodic" or unspecified (default is periodic):
        // intervalVariability must be "constant" or "fixed" → FAIL if it is "changing" or "countdown".
        if (clockType == "periodic")
        {
            if (intervalVariability == "changing" || intervalVariability == "countdown")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "Clock variable '{}' (line {}): is 'periodic' but has intervalVariability='{}'.",
                    name, node->line, intervalVariability));
            }
        }

        // Rule: If clockType="aperiodic" or clockType="triggered":
        // the attributes period, intervalCounter, shiftDecimal, and shiftFraction must not be present
        if (clockType == "aperiodic" || clockType == "triggered")
        {
            auto check_not_present = [&](const std::optional<std::string>& attr_val, const std::string& attr_name) {
                if (attr_val) {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format(
                        "Clock variable '{}' (line {}): attribute '{}' must not be present for clockType='{}'.",
                        name, node->line, attr_name, clockType));
                }
            };
            check_not_present(period, "period");
            check_not_present(intervalCounter, "intervalCounter");
            check_not_present(shiftDecimal, "shiftDecimal");
            check_not_present(shiftFraction, "shiftFraction");
        }

        // Rule: If clockType="triggered" and causality="output" → FAIL
        if (clockType == "triggered" && causality == "output")
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format(
                "Clock variable '{}' (line {}): triggered clocks cannot be outputs.",
                name, node->line));
        }

        // Rule: If shiftDecimal is present and period is also declared as a plain numeric attribute:
        // shiftDecimal must satisfy 0 <= shiftDecimal < period
        if (shiftDecimal && period)
        {
            auto sd_val = parseNumber<double>(*shiftDecimal);
            auto p_val = parseNumber<double>(*period);
            if (sd_val && p_val)
            {
                if (*sd_val < 0 || *sd_val >= *p_val)
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format(
                        "Clock variable '{}' (line {}): shiftDecimal ({}) must satisfy 0 <= shiftDecimal < period ({}).",
                        name, node->line, *shiftDecimal, *period));
                }
            }
        }

        // Rule: If both intervalCounter and resolution are present: resolution must be > 0
        if (intervalCounter && resolution)
        {
            auto res_val = parseNumber<double>(*resolution);
            if (res_val && *res_val <= 0)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "Clock variable '{}' (line {}): resolution must be > 0 when intervalCounter is present.",
                    name, node->line));
            }
        }

        // Rule: If clockType="countdown": causality must be "input" → FAIL if not.
        if (clockType == "countdown" && causality != "input")
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format(
                "Clock variable '{}' (line {}): clockType='countdown' requires causality='input' (found '{}').",
                name, node->line, causality));
        }
    }

    xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkCapabilityFlags(xmlDocPtr doc, Certificate& cert) const
{
    TestResult test{"Capability Flag Consistency", TestStatus::PASS, {}};

    auto check_interface = [&](xmlNodePtr node, const std::string& interface_name)
    {
        auto get_bool = [&](const std::string& attr, bool default_val) -> bool
        {
            auto val = getXmlAttribute(node, attr);
            if (!val)
                return default_val;
            return *val == "true" || *val == "1";
        };

        const bool canGetAndSetFMUState = get_bool("canGetAndSetFMUState", false);
        const bool canSerializeFMUState = get_bool("canSerializeFMUState", false);
        const bool providesDirectionalDerivatives = get_bool("providesDirectionalDerivatives", false);
        const bool providesAdjointDerivatives = get_bool("providesAdjointDerivatives", false);

        if (!canGetAndSetFMUState && canSerializeFMUState)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("{} (line {}): 'canSerializeFMUState' is true but 'canGetAndSetFMUState' is false. You "
                            "cannot serialize state you cannot get/set.",
                            interface_name, node->line));
        }

        if (!providesDirectionalDerivatives && providesAdjointDerivatives)
        {
            if (test.getStatus() != TestStatus::FAIL)
                test.setStatus(TestStatus::WARNING);
            test.getMessages().emplace_back(std::format(
                "{} (line {}): 'providesAdjointDerivatives' is true but 'providesDirectionalDerivatives' is false. "
                "Adjoint derivatives require directional derivative infrastructure in practice.",
                interface_name, node->line));
        }

        if (interface_name == "CoSimulation")
        {
            const bool hasEventMode = get_bool("hasEventMode", false);
            const bool canReturnEarlyAfterIntermediateUpdate = get_bool("canReturnEarlyAfterIntermediateUpdate", false);
            const bool providesIntermediateUpdate = get_bool("providesIntermediateUpdate", false);
            const bool mightReturnEarlyFromDoStep = get_bool("mightReturnEarlyFromDoStep", false);
            const bool canHandleVariableCommunicationStepSize = get_bool("canHandleVariableCommunicationStepSize", false);

            if (!hasEventMode && canReturnEarlyAfterIntermediateUpdate)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("CoSimulation (line {}): 'canReturnEarlyAfterIntermediateUpdate' is true but "
                                "'hasEventMode' is false.",
                                node->line));
            }

            if (!providesIntermediateUpdate && mightReturnEarlyFromDoStep)
            {
                if (test.getStatus() != TestStatus::FAIL)
                    test.setStatus(TestStatus::WARNING);
                test.getMessages().emplace_back(
                    std::format("CoSimulation (line {}): 'mightReturnEarlyFromDoStep' is true but "
                                "'providesIntermediateUpdate' is false.",
                                node->line));
            }

            if (canReturnEarlyAfterIntermediateUpdate && !providesIntermediateUpdate)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("CoSimulation (line {}): 'canReturnEarlyAfterIntermediateUpdate' is true but "
                                "'providesIntermediateUpdate' is false.",
                                node->line));
            }

            if (!hasEventMode && !canHandleVariableCommunicationStepSize)
            {
                if (test.getStatus() != TestStatus::FAIL)
                    test.setStatus(TestStatus::WARNING);
                test.getMessages().emplace_back(
                    std::format("CoSimulation (line {}): This FMU has neither event mode nor variable communication "
                                "step size, which indicates very limited co-simulation capability.",
                                node->line));
            }
        }
        else if (interface_name == "ScheduledExecution")
        {
            auto hasEventModeAttr = getXmlAttribute(node, "hasEventMode");
            if (hasEventModeAttr && (*hasEventModeAttr == "false" || *hasEventModeAttr == "0"))
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("ScheduledExecution (line {}): 'hasEventMode' must be true for Scheduled Execution.",
                                node->line));
            }
        }
    };

    const std::vector<std::string> interfaces = {"CoSimulation", "ModelExchange", "ScheduledExecution"};
    for (const auto& iface : interfaces)
    {
        xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//" + iface);
        if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
        {
            for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
                check_interface(node, iface);
            }
        }
        if (xpath_obj != nullptr)
            xmlXPathFreeObject(xpath_obj);
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkStructuralParameter(const std::vector<Variable>& variables,
                                                           Certificate& cert) const
{
    TestResult test{"Structural Parameter Validation", TestStatus::PASS, {}};

    // Build map of structural parameters by VR
    std::map<uint32_t, const Variable*> sp_map;
    for (const auto& var : variables)
    {
        if (var.causality == "structuralParameter")
        {
            if (var.value_reference.has_value())
                sp_map[*var.value_reference] = &var;

            // FMI3: Structural parameters must be UInt64
            if (var.type != "UInt64")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Structural parameter \"{}\" (line {}) must be of type UInt64, found {}.", var.name,
                                var.sourceline, var.type));
            }
        }
    }

    // Check variables with dimensions
    for (const auto& var : variables)
    {
        for (const auto& dim : var.dimensions)
        {
            if (dim.value_reference.has_value())
            {
                const uint32_t vr = *dim.value_reference;
                auto it = sp_map.find(vr);

                if (it == sp_map.end())
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(
                        std::format("Variable '{}' (line {}) references value reference {} in <Dimension> which is "
                                    "not a structural parameter.",
                                    var.name, var.sourceline, vr));
                }
                else
                {
                    const Variable* sp = it->second;
                    if (sp->start.has_value())
                    {
                        // Check that the structural parameter has start > 0
                        if (const auto start_val_opt = parseNumber<uint64_t>(*sp->start))
                        {
                            if (*start_val_opt == 0)
                            {
                                test.setStatus(TestStatus::FAIL);
                                test.getMessages().emplace_back(
                                    std::format("Structural parameter '{}' (line {}) is referenced in <Dimension> "
                                                "and must have start > 0.",
                                                sp->name, sp->sourceline));
                            }
                        }
                    }
                }
            }
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkModelStructure(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                      Certificate& cert) const
{
    validateOutputs(doc, variables, cert);
    validateDerivatives(doc, variables, cert);
    validateClockedStates(doc, variables, cert);
    validateInitialUnknowns(doc, variables, cert);
    validateEventIndicators(doc, variables, cert);
    checkVariableDependencies(doc, variables, cert);
}

void Fmi3ModelDescriptionChecker::validateOutputs(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                  Certificate& cert) const
{
    TestResult test{"ModelStructure Outputs", TestStatus::PASS, {}};

    // Get expected outputs (all variables with causality='output')
    std::set<uint32_t> expected_vrs;
    for (const auto& var : variables)
        if (var.causality == "output" && var.value_reference.has_value())
            expected_vrs.insert(*var.value_reference);

    // FMI3: Get actual outputs from 'ModelStructure/Output' (using valueReference attribute)
    std::set<uint32_t> actual_vrs;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/Output");

    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            const auto vr_str = getXmlAttribute(node, "valueReference");

            if (vr_str.has_value())
            {
                const auto vr_opt = parseNumber<uint32_t>(*vr_str);
                if (!vr_opt)
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(
                        std::format("ModelStructure/Output' {} has invalid valueReference '{}'.)", i + 1, *vr_str));
                    continue;
                }
                const uint32_t vr = *vr_opt;

                if (actual_vrs.contains(vr))
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(
                        std::format("Value reference {} is listed multiple times in 'ModelStructure/Output'.", vr));
                }
                actual_vrs.insert(vr);

                // Check if it's an output
                bool is_output = false;
                for (const auto& var : variables)
                {
                    if (var.value_reference.has_value() && *var.value_reference == vr)
                    {
                        if (var.causality == "output")
                        {
                            is_output = true;
                        }
                        else
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(
                                std::format("Variable '{}' (line {}) is listed in 'ModelStructure/Output' but does not "
                                            "have causality='output'.",
                                            var.name, var.sourceline));
                        }
                    }
                }

                if (!is_output && test.getStatus() != TestStatus::FAIL)
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format(
                        "Value reference {} is listed in 'ModelStructure/Output' but does not correspond to any "
                        "output variable.",
                        vr));
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    if (expected_vrs != actual_vrs)
    {
        test.setStatus(TestStatus::FAIL);

        for (const uint32_t vr : expected_vrs)
        {
            if (!actual_vrs.contains(vr))
            {
                test.getMessages().emplace_back(std::format(
                    "Output alias set (VR {}) is missing a representative in 'ModelStructure/Output'.", vr));
            }
        }

        test.getMessages().emplace_back("'ModelStructure/Output' must have exactly one representative for each alias "
                                        "set of variables with causality='output'.");
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::validateClockedStates(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                        Certificate& cert) const
{
    TestResult test{"ModelStructure Clocked States", TestStatus::PASS, {}};

    // Get expected clocked states: variables with causality="local" or "output" and has previous attribute
    std::set<uint32_t> expected_vrs;
    std::map<uint32_t, const Variable*> vr_to_var;
    for (const auto& var : variables)
    {
        if (var.value_reference.has_value())
            vr_to_var[*var.value_reference] = &var;

        if ((var.causality == "local" || var.causality == "output") && var.previous.has_value())
        {
            if (var.value_reference.has_value())
                expected_vrs.insert(*var.value_reference);
        }
    }

    // FMI3: Get actual clocked states from 'ModelStructure/ClockedState'
    std::set<uint32_t> actual_vrs;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/ClockedState");

    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            const auto vr_str = getXmlAttribute(node, "valueReference");

            if (vr_str.has_value())
            {
                const auto vr_opt = parseNumber<uint32_t>(*vr_str);
                if (!vr_opt)
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format(
                        "'ModelStructure/ClockedState' {} has invalid valueReference '{}'.", i + 1, *vr_str));
                    continue;
                }
                const uint32_t vr = *vr_opt;

                if (actual_vrs.contains(vr))
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format(
                        "Value reference {} is listed multiple times in 'ModelStructure/ClockedState'.", vr));
                }
                actual_vrs.insert(vr);

                auto it = vr_to_var.find(vr);
                if (it != vr_to_var.end())
                {
                    const Variable& var = *it->second;

                    if (var.variability != "discrete")
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(
                            std::format("Clocked state variable '{}' (line {}) must have variability='discrete'.",
                                        var.name, var.sourceline));
                    }

                    if (!var.clocks.has_value() || var.clocks->empty())
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(
                            std::format("Clocked state variable '{}' (line {}) must have the 'clocks' attribute.",
                                        var.name, var.sourceline));
                    }

                    if (var.type == "Clock")
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(
                            std::format("Clocked state variable '{}' (line {}) must not be of type Clock.", var.name,
                                        var.sourceline));
                    }
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    if (expected_vrs != actual_vrs)
    {
        test.setStatus(TestStatus::FAIL);
        std::vector<std::string> missing;
        for (const uint32_t vr : expected_vrs)
            if (!actual_vrs.contains(vr))
                missing.push_back(vr_to_var[vr]->name + " (VR " + std::to_string(vr) + ")");

        if (!missing.empty())
        {
            std::string msg = "The following clocked states are missing from 'ModelStructure/ClockedState': ";
            for (size_t i = 0; i < missing.size(); ++i)
                msg += (i > 0 ? ", " : "") + missing[i];
            msg += ".";
            test.getMessages().emplace_back(msg);
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::validateDerivatives(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                      Certificate& cert) const
{
    TestResult test{"ModelStructure Derivatives", TestStatus::PASS, {}};

    std::map<uint32_t, const Variable*> vr_map;
    for (const auto& var : variables)
        if (var.value_reference.has_value())
            vr_map[*var.value_reference] = &var;

    // FMI3: Check ContinuousStateDerivative entries (using valueReference attribute)
    std::set<uint32_t> actual_vrs;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/ContinuousStateDerivative");

    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            const auto vr_str = getXmlAttribute(node, "valueReference");

            if (vr_str.has_value())
            {
                const auto vr_opt = parseNumber<uint32_t>(*vr_str);
                if (!vr_opt)
                    continue;
                const uint32_t vr = *vr_opt;

                if (actual_vrs.contains(vr))
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format(
                        "Value reference {} is listed multiple times in 'ModelStructure/ContinuousStateDerivative'.",
                        vr));
                }
                actual_vrs.insert(vr);

                auto it = vr_map.find(vr);
                if (it == vr_map.end())
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format("'ModelStructure/ContinuousStateDerivative' references "
                                                                "non-existent valueReference {}.",
                                                                vr));
                }
                else
                {
                    const Variable& var = *it->second;
                    if (!var.derivative_of.has_value())
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(
                            std::format("Variable '{}' (VR {}) listed in 'ModelStructure/ContinuousStateDerivative' "
                                        "must have a 'derivative' attribute.",
                                        var.name, vr));
                    }
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkDerivativeDimensions(const std::vector<Variable>& variables,
                                                            Certificate& cert) const
{
    TestResult test{"Derivative Dimension Matching", TestStatus::PASS, {}};

    // Build a map of value_reference -> Variable for quick lookup
    std::map<uint32_t, const Variable*> vr_to_variable;
    for (const auto& var : variables)
        if (var.value_reference.has_value())
            vr_to_variable[*var.value_reference] = &var;

    // Check each variable that has a derivative_of attribute
    for (const auto& var : variables)
    {
        if (var.derivative_of.has_value())
        {
            const uint32_t derivative_of_vr = *var.derivative_of;

            // Find the state variable
            auto it = vr_to_variable.find(derivative_of_vr);
            if (it == vr_to_variable.end())
            {
                // This will be caught by checkDerivativeReferences, so skip here
                continue;
            }

            const Variable* state_var = it->second;

            // Compare dimensions
            const bool dimensions_match = compareDimensions(var, *state_var);

            if (!dimensions_match)
            {
                test.setStatus(TestStatus::FAIL);

                const std::string derivative_dims = formatDimensions(var);
                const std::string state_dims = formatDimensions(*state_var);

                test.getMessages().emplace_back(std::format(
                    "Variable '{}' (line {}) is derivative of '{}' (line {}) but has different dimensions. Derivative "
                    "dimensions: {}, State dimensions: {}.",
                    var.name, var.sourceline, state_var->name, state_var->sourceline, derivative_dims, state_dims));
            }
        }
    }

    cert.printTestResult(test);
}

// Helper function to compare dimensions between two variables
bool Fmi3ModelDescriptionChecker::compareDimensions(const Variable& var1, const Variable& var2) const
{
    // If one is an array and the other is not, they don't match
    if (var1.dimensions.empty() != var2.dimensions.empty())
        return false;

    // Both are scalars (no dimensions)
    if (var1.dimensions.empty() && var2.dimensions.empty())
        return true;

    // Both are arrays - must have same number of dimensions
    if (var1.dimensions.size() != var2.dimensions.size())
        return false;

    // Compare each dimension
    for (size_t i = 0; i < var1.dimensions.size(); ++i)
    {
        const auto& dim1 = var1.dimensions[i];
        const auto& dim2 = var2.dimensions[i];

        // Case 1: Both have fixed start values - must be equal
        if (dim1.start.has_value() && dim2.start.has_value())
        {
            if (*dim1.start != *dim2.start)
                return false;
        }
        // Case 2: Both reference value references - must reference the same parameter
        else if (dim1.value_reference.has_value() && dim2.value_reference.has_value())
        {
            if (*dim1.value_reference != *dim2.value_reference)
                return false;
        }
        // Case 3: One has fixed start, other has reference - they don't match
        // (even if the reference evaluates to the same value, structurally they're different)
        else
        {
            return false;
        }
    }

    return true;
}

// Helper function to format dimensions for error messages
std::string Fmi3ModelDescriptionChecker::formatDimensions(const Variable& var) const
{
    if (var.dimensions.empty())
        return "scalar";

    std::string result = "[";
    for (size_t i = 0; i < var.dimensions.size(); ++i)
    {
        if (i > 0)
            result += ", ";

        const auto& dim = var.dimensions[i];
        if (dim.start.has_value())
            result += std::to_string(*dim.start);
        else if (dim.value_reference.has_value())
            result += "vr:" + std::to_string(*dim.value_reference);
        else
            result += "?";
    }
    result += "]";

    return result;
}

void Fmi3ModelDescriptionChecker::checkVariableDependencies(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                            Certificate& cert) const
{
    TestResult test{"Variable Dependencies", TestStatus::PASS, {}};

    // Build a map for lookup
    std::map<uint32_t, const Variable*> vr_to_var;
    for (const auto& var : variables)
        if (var.value_reference.has_value())
            vr_to_var[*var.value_reference] = &var;

    auto check_deps = [&](xmlNodePtr node, const std::string& elem_name, bool is_initial_unknown)
    {
        auto vr_str = getXmlAttribute(node, "valueReference");
        auto deps_str = getXmlAttribute(node, "dependencies");
        auto kinds_str = getXmlAttribute(node, "dependenciesKind");

        if (!vr_str)
            return;

        uint32_t unknown_vr = 0;
        const auto unknown_vr_opt = parseNumber<uint32_t>(*vr_str);
        if (!unknown_vr_opt)
            return;
        unknown_vr = *unknown_vr_opt;

        const Variable* unknown_var = nullptr;
        if (vr_to_var.contains(unknown_vr))
            unknown_var = vr_to_var[unknown_vr];

        // 1. If dependenciesKind is present, dependencies must be present
        if (kinds_str && !deps_str)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("{} (VR {}) has 'dependenciesKind' but 'dependencies' is missing.", elem_name, unknown_vr));
        }

        if (deps_str)
        {
            std::vector<uint32_t> deps;
            std::stringstream ss_deps(*deps_str);
            std::string item;
            while (ss_deps >> item)
                if (const auto dep_vr = parseNumber<uint32_t>(item))
                    deps.push_back(*dep_vr);

            if (kinds_str)
            {
                std::vector<std::string> kinds;
                std::stringstream ss_kinds(*kinds_str);
                while (ss_kinds >> item)
                    kinds.push_back(item);

                // 2. dependencies and dependenciesKind must have the same number of elements
                if (deps.size() != kinds.size())
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format("{} (VR {}) has different number of elements in "
                                                                "'dependencies' ({}) and 'dependenciesKind' ({}).",
                                                                elem_name, unknown_vr, deps.size(), kinds.size()));
                }

                for (const auto& k : kinds)
                {
                    // 4. 'fixed', 'tunable', 'discrete' only for floating point unknowns AND NOT for InitialUnknown
                    if (k == "fixed" || k == "tunable" || k == "discrete")
                    {
                        if (is_initial_unknown)
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(
                                std::format("{} (VR {}) has illegal dependencyKind '{}' (not "
                                            "allowed for InitialUnknown).",
                                            elem_name, unknown_vr, k));
                        }
                        else if (unknown_var && unknown_var->type != "Float32" && unknown_var->type != "Float64")
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(
                                std::format("{} (VR {}) has dependencyKind '{}' but unknown is not "
                                            "a float type.",
                                            elem_name, unknown_vr, k));
                        }
                    }
                }
            }

            // 5. Check that all dependency VRs exist
            for (const uint32_t d_vr : deps)
            {
                if (!vr_to_var.contains(d_vr))
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format("{} (VR {}) references non-existent dependency VR {}.",
                                                                elem_name, unknown_vr, d_vr));
                }
            }
        }
    };

    const std::vector<std::pair<std::string, bool>> structure_elems = {
        {"//ModelStructure/Output", false},
        {"//ModelStructure/ContinuousStateDerivative", false},
        {"//ModelStructure/InitialUnknown", true}};

    for (const auto& [xpath, is_initial] : structure_elems)
    {
        xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, xpath);
        if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
        {
            for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
                check_deps(node, xpath.substr(xpath.find_last_of('/') + 1), is_initial);
            }
            xmlXPathFreeObject(xpath_obj);
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::validateEventIndicators(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                          Certificate& cert) const
{
    TestResult test{"ModelStructure Event Indicators", TestStatus::PASS, {}};

    // FMI 3.0: 'ModelStructure/EventIndicator' elements define the event indicators.
    // There is no numberOfEventIndicators attribute on the root element.

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/EventIndicator");

    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        std::set<uint32_t> seen_vrs;
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            auto vr_str = getXmlAttribute(node, "valueReference");

            if (vr_str.has_value())
            {
                const auto vr_opt = parseNumber<uint32_t>(*vr_str);
                if (!vr_opt)
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format("{} (VR {}) has invalid valueReference '{}'.",
                                                                "'ModelStructure/EventIndicator'", i + 1, *vr_str));
                    continue;
                }
                const uint32_t vr = *vr_opt;

                if (seen_vrs.contains(vr))
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format(
                        "Value reference {} is listed multiple times in 'ModelStructure/EventIndicator'.", vr));
                }
                seen_vrs.insert(vr);

                // Check if the variable exists and is legal for an event indicator
                bool found = false;
                for (const auto& var : variables)
                {
                    if (var.value_reference.has_value() && *var.value_reference == vr)
                    {
                        found = true;
                        // Continuous-time state or an event indicator must have causality = local or output
                        if (var.causality != "local" && var.causality != "output")
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(std::format("{} (VR {}) is used as an event indicator but "
                                                                        "does not have causality='local' or 'output'.",
                                                                        "'ModelStructure/EventIndicator'", i + 1));
                        }

                        // Only continuous variables of type Float32 and Float64 can be referenced by EventIndicator
                        if (var.type != "Float32" && var.type != "Float64")
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(std::format("{} (VR {}) is used as an event indicator but "
                                                                        "is of type {} (must be Float32 or Float64).",
                                                                        "'ModelStructure/EventIndicator'", i + 1,
                                                                        var.type));
                        }

                        if (var.variability != "continuous")
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(std::format(
                                "{} (VR {}) is used as an event indicator but does not have variability='continuous'.",
                                "'ModelStructure/EventIndicator'", i + 1));
                        }
                        break;
                    }
                }

                if (!found)
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format("{} (VR {}) references non-existent valueReference {}.",
                                                                "'ModelStructure/EventIndicator'", i + 1, vr));
                }
            }
            else
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("{} (VR {}) is missing the mandatory 'valueReference' attribute.",
                                "'ModelStructure/EventIndicator'", i + 1));
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::validateInitialUnknowns(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                          Certificate& cert) const
{
    TestResult test{"ModelStructure Initial Unknowns", TestStatus::PASS, {}};

    // Identify active continuous-time states and derivatives
    std::set<uint32_t> active_state_vrs;
    std::set<uint32_t> active_derivative_vrs;

    xmlXPathObjectPtr xpath_der = getXPathNodes(doc, "//ModelStructure/ContinuousStateDerivative");
    if (xpath_der != nullptr && xpath_der->nodesetval != nullptr)
    {
        std::map<uint32_t, const Variable*> vr_to_var;
        for (const auto& v : variables)
            if (v.value_reference.has_value())
                vr_to_var[*v.value_reference] = &v;

        for (int32_t i = 0; i < xpath_der->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = xpath_der->nodesetval->nodeTab[i];
            const auto vr_str = getXmlAttribute(node, "valueReference");
            if (vr_str.has_value())
            {
                if (const auto vr_opt = parseNumber<uint32_t>(*vr_str))
                {
                    const uint32_t vr = *vr_opt;
                    active_derivative_vrs.insert(vr);
                    auto it = vr_to_var.find(vr);
                    if (it != vr_to_var.end())
                    {
                        const auto& var_ref = *it->second;
                        if (var_ref.derivative_of.has_value())
                            active_state_vrs.insert(var_ref.derivative_of.value());
                    }
                }
            }
        }
        xmlXPathFreeObject(xpath_der);
    }

    // Build expected set of initial unknown value references (FMI3 spec)
    std::set<uint32_t> expected_vrs;

    for (const auto& var : variables)
    {
        if (!var.value_reference.has_value())
            continue;

        bool is_required = false;

        // Mandatory unknowns according to FMI 3.0 spec:
        // (1) Outputs with initial="approx" or "calculated" (not clocked)
        // (2) Calculated parameters
        // (3) Active state derivatives with initial="approx" or "calculated"
        // (4) Active states with initial="approx" or "calculated"
        if ((var.causality == "output" && (var.initial == "approx" || var.initial == "calculated") &&
             (!var.clocks.has_value() || var.clocks->empty())) ||
            (var.causality == "calculatedParameter") ||
            ((active_derivative_vrs.contains(*var.value_reference) ||
              active_state_vrs.contains(*var.value_reference)) &&
             (var.initial == "approx" || var.initial == "calculated")))
        {
            is_required = true;
        }

        if (is_required)
            expected_vrs.insert(*var.value_reference);
    }

    // FMI3: Get actual initial unknowns (using valueReference attribute)
    std::set<uint32_t> actual_vrs;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/InitialUnknown");

    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            auto vr_str = getXmlAttribute(node, "valueReference");

            if (vr_str.has_value())
            {
                if (const auto vr_opt = parseNumber<uint32_t>(*vr_str))
                {
                    const uint32_t vr = *vr_opt;
                    if (actual_vrs.contains(vr))
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(std::format(
                            "Value reference {} is listed multiple times in 'ModelStructure/InitialUnknown'.", vr));
                    }
                    actual_vrs.insert(vr);
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    // Mandatory: expected_vrs (contains non-clocked outputs, calculated parameters, states/derivatives)
    // Optional: clocked variables
    std::map<uint32_t, const Variable*> vr_to_variable;
    for (const auto& var : variables)
        if (var.value_reference.has_value())
            vr_to_variable[*var.value_reference] = &var;

    for (const uint32_t vr : expected_vrs)
    {
        if (!actual_vrs.contains(vr))
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Mandatory initial unknown alias set (VR {}) is missing a representative in "
                            "'ModelStructure/InitialUnknown'.",
                            vr));
        }
    }

    for (const uint32_t vr : actual_vrs)
    {
        if (!expected_vrs.contains(vr))
        {
            // It might be an optional clocked variable
            bool is_clocked = false;
            auto it = vr_to_variable.find(vr);
            if (it != vr_to_variable.end())
            {
                const auto& var_obj = *it->second;
                if (var_obj.clocks.has_value() && !var_obj.clocks.value().empty())
                    is_clocked = true;
            }

            if (!is_clocked)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Variable (VR {}) in 'ModelStructure/InitialUnknown' is not allowed (only "
                                "mandatory unknowns and optional clocked variables are allowed).",
                                vr));
            }
        }
    }

    if (test.getStatus() == TestStatus::FAIL)
    {
        test.getMessages().emplace_back(
            "'ModelStructure/InitialUnknown' must have exactly one representative for each mandatory "
            "alias set. Optional clocked variables are also allowed.");
    }

    cert.printTestResult(test);
}

std::map<std::string, TypeDefinition> Fmi3ModelDescriptionChecker::extractTypeDefinitions(xmlDocPtr doc) const
{
    std::map<std::string, TypeDefinition> type_definitions;

    // FMI3: Type definitions are direct children of TypeDefinitions (Float32Type, Float64Type, Int8Type, etc.)
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//TypeDefinitions/*");
    if (xpath_obj == nullptr)
        return type_definitions;

    xmlNodeSetPtr nodes = xpath_obj->nodesetval;
    if (nodes == nullptr)
    {
        xmlXPathFreeObject(xpath_obj);
        return type_definitions;
    }

    for (int32_t i = 0; i < nodes->nodeNr; ++i)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr type_node = nodes->nodeTab[i];
        TypeDefinition type_def;

        // Get name attribute
        type_def.name = getXmlAttribute(type_node, "name").value_or("");
        type_def.sourceline = type_node->line;

        // Element name IS the type (Float32Type, Int8Type, BooleanType, etc.)
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const std::string elem_name = reinterpret_cast<const char*>(type_node->name);

        // Strip "Type" suffix to get the actual type name
        if (elem_name.length() > 4 && elem_name.substr(elem_name.length() - 4) == "Type")
            type_def.type = elem_name.substr(0, elem_name.length() - 4);
        else
            type_def.type = elem_name;

        // Extract attributes from the type element itself
        type_def.min = getXmlAttribute(type_node, "min");
        type_def.max = getXmlAttribute(type_node, "max");
        type_def.nominal = getXmlAttribute(type_node, "nominal");
        type_def.unit = getXmlAttribute(type_node, "unit");
        type_def.display_unit = getXmlAttribute(type_node, "displayUnit");

        if (!type_def.name.empty())
            type_definitions[type_def.name] = type_def;
    }

    xmlXPathFreeObject(xpath_obj);
    return type_definitions;
}

void Fmi3ModelDescriptionChecker::extractDimensions(xmlNodePtr node, Variable& var) const
{
    // Look for Dimension child elements
    for (xmlNodePtr child = node->children; child != nullptr; child = child->next)
    {
        if (child->type == XML_ELEMENT_NODE &&
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            xmlStrcmp(child->name, reinterpret_cast<const xmlChar*>("Dimension")) == 0)
        {
            var.has_dimension = true;

            Dimension dim;
            dim.sourceline = child->line;

            // Extract start attribute (fixed dimension size)
            auto start_attr = getXmlAttribute(child, "start");
            if (start_attr.has_value())
                dim.start = parseNumber<uint64_t>(*start_attr);

            // Extract valueReference attribute (reference to structural parameter)
            auto vr_attr = getXmlAttribute(child, "valueReference");
            if (vr_attr.has_value())
                dim.value_reference = parseNumber<uint32_t>(*vr_attr);

            var.dimensions.push_back(dim);
        }
    }
}

void Fmi3ModelDescriptionChecker::checkDimensionReferences(const std::vector<Variable>& variables,
                                                           Certificate& cert) const
{
    TestResult test{"Dimension References", TestStatus::PASS, {}};

    // Build a map of all variables by valueReference
    std::map<uint32_t, const Variable*> vars_by_vr;
    for (const auto& var : variables)
        if (var.value_reference.has_value())
            vars_by_vr[*var.value_reference] = &var;

    // Check each variable with dimensions
    for (const auto& var : variables)
    {
        if (!var.dimensions.empty())
        {
            for (size_t i = 0; i < var.dimensions.size(); ++i)
            {
                const auto& dim = var.dimensions[i];

                // A dimension must have either start OR valueReference, but not both
                const bool has_start = dim.start.has_value();
                const bool has_vr = dim.value_reference.has_value();

                if (!has_start && !has_vr)
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(
                        std::format("Variable '{}' (line {}), Dimension {} (line {}): must have either 'start' or "
                                    "'valueReference' attribute.",
                                    var.name, var.sourceline, i + 1, dim.sourceline));
                }
                else if (has_start && has_vr)
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(
                        std::format("Variable '{}' (line {}), Dimension {} (line {}): must have either 'start' OR "
                                    "'valueReference', not both.",
                                    var.name, var.sourceline, i + 1, dim.sourceline));
                }

                // If valueReference is used, check that it points to a structural parameter
                if (has_vr)
                {
                    const uint32_t vr = *dim.value_reference;
                    auto it = vars_by_vr.find(vr);

                    if (it == vars_by_vr.end())
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(std::format(
                            "Variable '{}' (line {}), Dimension {} (line {}): references value reference {} "
                            "which does not exist.",
                            var.name, var.sourceline, i + 1, dim.sourceline, vr));
                    }
                    else
                    {
                        const Variable* sp = it->second;

                        // Rule: resolve it to a variable. That variable must have causality="structuralParameter"
                        if (sp->causality != "structuralParameter")
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(std::format(
                                "Variable '{}' (line {}), Dimension {} (line {}): references variable '{}' (VR {}) "
                                "which has causality='{}' (must be 'structuralParameter').",
                                var.name, var.sourceline, i + 1, dim.sourceline, sp->name, vr, sp->causality));
                        }

                        // Rule: the referenced variable must be of type UInt64
                        if (sp->type != "UInt64")
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(
                                std::format("Variable '{}' (line {}), Dimension {} (line {}): references structural "
                                            "parameter '{}' (VR {}) which has type '{}' (must be 'UInt64').",
                                            var.name, var.sourceline, i + 1, dim.sourceline, sp->name, vr, sp->type));
                        }

                        // Rule: only scalar variables (no <Dimension> children) can be dimension references
                        if (sp->has_dimension)
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(std::format(
                                "Variable '{}' (line {}), Dimension {} (line {}): references structural parameter '{}' "
                                "(VR {}) which is itself an array. Only scalar structural parameters can be used as "
                                "dimension references.",
                                var.name, var.sourceline, i + 1, dim.sourceline, sp->name, vr));
                        }

                        // Rule: If a <Dimension> uses valueReference and the referenced structural parameter has
                        // variability="fixed", but its start is 0 → FAIL
                        if (sp->start.has_value())
                        {
                            if (const auto start_val_opt = parseNumber<uint64_t>(*sp->start))
                            {
                                if (*start_val_opt == 0)
                                {
                                    test.setStatus(TestStatus::FAIL);
                                    test.getMessages().emplace_back(std::format(
                                        "Variable '{}' (line {}), Dimension {} (line {}): references structural "
                                        "parameter '{}' (line {}): has start=0 (must be > 0).",
                                        var.name, var.sourceline, i + 1, dim.sourceline, sp->name, sp->sourceline));
                                }
                            }
                            else
                            {
                                test.setStatus(TestStatus::FAIL);
                                test.getMessages().emplace_back(std::format(
                                    "Variable '{}' (line {}), Dimension {} (line {}): references structural "
                                    "parameter '{}' (line {}): has invalid start value (not a valid UInt64).",
                                    var.name, var.sourceline, i + 1, dim.sourceline, sp->name, sp->sourceline));
                            }
                        }
                        else
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(
                                std::format("Variable '{}' (line {}), Dimension {} (line {}): references structural "
                                            "parameter '{}' (line {}): does not have a start value.",
                                            var.name, var.sourceline, i + 1, dim.sourceline, sp->name, sp->sourceline));
                        }
                    }
                }

                // If start is used directly, check that it's > 0
                if (has_start && !has_vr)
                {
                    if (*dim.start == 0)
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(
                            std::format("Variable '{}' (line {}), Dimension {} (line {}): has start=0 (must be > 0).",
                                        var.name, var.sourceline, i + 1, dim.sourceline));
                    }
                }
            }
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkArrayStartValues(const std::vector<Variable>& variables, Certificate& cert) const
{
    TestResult test{"Array Start Values", TestStatus::PASS, {}};

    // Build a map of value_reference -> Variable for structural parameters
    std::map<uint32_t, const Variable*> structural_params_by_vr;
    for (const auto& var : variables)
        if (var.causality == "structuralParameter" && var.value_reference.has_value())
            structural_params_by_vr[*var.value_reference] = &var;

    // Check each variable with dimensions that has a start value
    for (const auto& var : variables)
    {
        if (!var.dimensions.empty() && var.start.has_value())
        {
            // Calculate the expected total array size
            std::optional<uint64_t> total_size = 1;
            bool size_determinable = true;
            std::vector<std::string> dimension_info;

            for (const auto& dim : var.dimensions)
            {
                if (dim.start.has_value())
                {
                    // Fixed dimension size
                    total_size = *total_size * (*dim.start);
                    dimension_info.push_back(std::to_string(*dim.start));
                }
                else if (dim.value_reference.has_value())
                {
                    // Dimension from structural parameter
                    auto it = structural_params_by_vr.find(*dim.value_reference);
                    if (it != structural_params_by_vr.end())
                    {
                        const Variable* sp = it->second;
                        if (sp->start.has_value())
                        {
                            if (const auto dim_size_opt = parseNumber<uint64_t>(*sp->start))
                            {
                                total_size = *total_size * (*dim_size_opt);
                                dimension_info.push_back(sp->name + "=" + std::to_string(*dim_size_opt));
                            }
                            else
                            {
                                // Invalid structural parameter value - skip this check
                                // (will be caught by dimension reference check)
                                size_determinable = false;
                                break;
                            }
                        }
                        else
                        {
                            // Structural parameter has no start value
                            // (will be caught by dimension reference check)
                            size_determinable = false;
                            break;
                        }
                    }
                    else
                    {
                        // Invalid reference (will be caught by dimension reference check)
                        size_determinable = false;
                        break;
                    }
                }
                else
                {
                    // Invalid dimension (will be caught by dimension reference check)
                    size_determinable = false;
                    break;
                }
            }

            // If we can determine the size, count the start values
            if (size_determinable && total_size.has_value())
            {
                const size_t num_start_values = var.num_start_values;

                // Check if the count matches
                if (num_start_values != *total_size && num_start_values != 1)
                {
                    // Either must match exactly, or be a single scalar value (broadcast)
                    std::string dim_str;
                    for (size_t i = 0; i < dimension_info.size(); ++i)
                    {
                        if (i > 0)
                            dim_str += " x ";
                        dim_str += dimension_info[i];
                    }

                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format(
                        "Variable '{}' (line {}): is an array with dimensions [{}] (total size {}): but has {} start "
                        "value(s). Expected either {} values or 1 scalar value (for broadcast).",
                        var.name, var.sourceline, dim_str, std::to_string(*total_size),
                        std::to_string(num_start_values), std::to_string(*total_size)));
                }
            }
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkClockReferences(const std::vector<Variable>& variables, Certificate& cert) const
{
    TestResult test{"Check Clock References", TestStatus::PASS, {}};

    // Build a map of value references to variables for efficient lookup
    std::map<uint32_t, const Variable*> vr_to_var;

    for (const auto& var : variables)
        if (var.value_reference.has_value())
            vr_to_var[*var.value_reference] = &var;

    // Check each variable that has a clocks attribute
    for (const auto& var : variables)
    {
        if (!var.clocks.has_value() || var.clocks->empty())
            continue;

        // Parse the space-separated list of clock value references
        std::stringstream ss(*var.clocks);
        std::string vr_str;
        std::vector<uint32_t> clock_refs;

        while (ss >> vr_str)
        {
            if (const auto vr_opt = parseNumber<uint32_t>(vr_str))
            {
                clock_refs.push_back(*vr_opt);
            }
            else
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Variable '{}' (line {}): Invalid clock reference '{}' in clocks attribute.", var.name,
                                var.sourceline, vr_str));
                continue;
            }
        }

        // Validate each clock reference
        for (const uint32_t clock_vr : clock_refs)
        {
            // Check if a Clock is referencing itself
            if (var.type == "Clock" && var.value_reference.has_value() && *var.value_reference == clock_vr)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Variable '{}' (line {}): Clock cannot reference itself.", var.name, var.sourceline));
                continue;
            }

            // Check if the value reference exists
            auto it = vr_to_var.find(clock_vr);
            if (it == vr_to_var.end())
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Variable '{}' (line {}): References non-existent clock with valueReference {}.",
                                var.name, var.sourceline, clock_vr));
                continue;
            }

            // Check if the referenced variable is actually a Clock
            if (it->second->type != "Clock")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format("Variable '{}' (line {}): References valueReference {} "
                                                            "which is a {}, not a Clock (variable '{}', line {}).",
                                                            var.name, var.sourceline, clock_vr, it->second->type,
                                                            it->second->name, it->second->sourceline));
            }
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkClockedVariables(const std::vector<Variable>& variables, Certificate& cert) const
{
    TestResult test{"Check Clocked Variables", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        // Skip variables without clocks attribute
        if (!var.clocks.has_value() || var.clocks->empty())
            continue;

        // Note: Clock variables CAN have a clocks attribute (per FMI3 spec section 2.2.8.3)

        // Check causality - clocked variables must have specific causality values
        if (var.causality != "input" && var.causality != "output" && var.causality != "local")
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format("Variable '{}' (line {}): Clocked variables must have "
                                                        "causality 'input', 'output', or 'local', but has '{}'.",
                                                        var.name, var.sourceline, var.causality));
        }

        // Check variability - clocked variables must have discrete variability
        if (var.variability != "discrete")
        {
            // Continuous variables cannot be clocked in general
            if (var.variability == "continuous")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Variable '{}' (line {}): Continuous variables cannot have a clocks attribute. Clocked "
                                "variables must have variability='discrete'.",
                                var.name, var.sourceline));
            }
            // Constants, fixed, and tunable also cannot be clocked
            else if (var.variability == "constant" || var.variability == "fixed" || var.variability == "tunable")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Variable '{}' (line {}): Variables with variability='{}' cannot have a clocks "
                                "attribute. Clocked variables must have variability='discrete'.",
                                var.name, var.sourceline, var.variability));
            }
        }

        // Check that independent variable is not clocked
        if (var.causality == "independent")
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Variable '{}' (line {}): Independent variable cannot have a clocks attribute.", var.name,
                            var.sourceline));
        }

        // Check that parameters are not clocked
        if (var.causality == "parameter" || var.causality == "calculatedParameter" ||
            var.causality == "structuralParameter")
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Variable '{}' (line {}): Parameters (causality='{}') cannot have a clocks attribute.",
                            var.name, var.sourceline, var.causality));
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::validateVariableSpecialFloat(TestResult& /*test*/, const Variable& /*var*/,
                                                               const std::string& /*val*/,
                                                               const std::string& /*attr_name*/) const
{
    // Special floats are allowed in FMI 3.0 variable values
}

void Fmi3ModelDescriptionChecker::validateDefaultExperimentSpecialFloat(TestResult& /*test*/,
                                                                        const std::string& /*val*/,
                                                                        const std::string& /*attr_name*/) const
{
    // Special floats are allowed in FMI 3.0
}

void Fmi3ModelDescriptionChecker::validateUnitSpecialFloat(TestResult& /*test*/, const std::string& /*val*/,
                                                           const std::string& /*attr_name*/,
                                                           const std::string& /*context*/, size_t /*line*/) const
{
    // Special floats are allowed in FMI 3.0
}

void Fmi3ModelDescriptionChecker::validateTypeDefinitionSpecialFloat(TestResult& /*test*/,
                                                                     const TypeDefinition& /*type_def*/,
                                                                     const std::string& /*val*/,
                                                                     const std::string& /*attr_name*/) const
{
    // Special floats are allowed in FMI 3.0 type definitions
}

void Fmi3ModelDescriptionChecker::checkTypeDefinitions(xmlDocPtr doc, Certificate& cert) const
{
    TestResult test{"Type Definitions", TestStatus::PASS, {}};

    // FMI3: Type definitions are direct children of TypeDefinitions
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//TypeDefinitions/*");
    if (xpath_obj == nullptr)
    {
        cert.printTestResult(test);
        return;
    }

    xmlNodeSetPtr nodes = xpath_obj->nodesetval;
    if (nodes == nullptr)
    {
        xmlXPathFreeObject(xpath_obj);
        cert.printTestResult(test);
        return;
    }

    std::set<std::string> seen_names;

    for (int32_t i = 0; i < nodes->nodeNr; ++i)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr type_node = nodes->nodeTab[i];

        if (type_node->type != XML_ELEMENT_NODE)
            continue;

        auto name_opt = getXmlAttribute(type_node, "name");
        const std::string name = name_opt.value_or("unnamed");

        if (name_opt)
        {
            if (seen_names.contains(*name_opt))
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "Type definition '{}' (line {}): is defined multiple times.", *name_opt, type_node->line));
            }
            seen_names.insert(*name_opt);
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const std::string elem_name = reinterpret_cast<const char*>(type_node->name);

        auto min_str = getXmlAttribute(type_node, "min");
        auto max_str = getXmlAttribute(type_node, "max");

        if (min_str && max_str)
        {
            const bool special_min = isSpecialFloat(*min_str);
            const bool special_max = isSpecialFloat(*max_str);

            if (!special_min && !special_max)
            {
                if (elem_name == "Float32Type" || elem_name == "Float64Type")
                {
                    const auto min_val = parseNumber<double>(*min_str);
                    const auto max_val = parseNumber<double>(*max_str);
                    if (min_val && max_val && *max_val < *min_val)
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(
                            std::format("Type definition '{}' (line {}): max ({}) must be >= min ({}).", name,
                                        type_node->line, *max_str, *min_str));
                    }
                }
                else if (elem_name.find("UInt") != std::string::npos)
                {
                    const auto min_val = parseNumber<uint64_t>(*min_str);
                    const auto max_val = parseNumber<uint64_t>(*max_str);
                    if (min_val && max_val && *max_val < *min_val)
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(
                            std::format("Type definition '{}' (line {}): max ({}) must be >= min ({}).", name,
                                        type_node->line, *max_str, *min_str));
                    }
                }
                else if (elem_name.find("Int") != std::string::npos || elem_name == "EnumerationType")
                {
                    const auto min_val = parseNumber<int64_t>(*min_str);
                    const auto max_val = parseNumber<int64_t>(*max_str);
                    if (min_val && max_val && *max_val < *min_val)
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(
                            std::format("Type definition '{}' (line {}): max ({}) must be >= min ({}).", name,
                                        type_node->line, *max_str, *min_str));
                    }
                }
            }
        }

        if (elem_name == "EnumerationType")
        {
            bool has_items = false;
            std::set<int64_t> item_values;
            std::set<std::string> item_names;

            for (xmlNodePtr item = type_node->children; item != nullptr; item = item->next)
            {
                if (item->type != XML_ELEMENT_NODE)
                    continue;

                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                const std::string item_elem_name = reinterpret_cast<const char*>(item->name);
                if (item_elem_name == "Item")
                {
                    has_items = true;
                    auto item_name = getXmlAttribute(item, "name");
                    auto item_value_str = getXmlAttribute(item, "value");

                    if (item_name)
                    {
                        if (item_names.contains(*item_name))
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(
                                std::format("Enumeration type '{}' (line {}): has multiple items named '{}'.", name,
                                            type_node->line, *item_name));
                        }
                        item_names.insert(*item_name);
                    }

                    if (item_value_str)
                    {
                        if (const auto val_opt = parseNumber<int64_t>(*item_value_str))
                        {
                            const int64_t val = *val_opt;
                            if (item_values.contains(val))
                            {
                                test.setStatus(TestStatus::FAIL);
                                test.getMessages().emplace_back(
                                    std::format("Enumeration type '{}' (line {}): has multiple items with value '{}'. "
                                                "Item values must be unique.",
                                                name, type_node->line, *item_value_str));
                            }
                            item_values.insert(val);
                        }
                    }
                }
            }

            if (!has_items)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "Enumeration type '{}' (line {}): must have at least one Item.", name, type_node->line));
            }
        }
    }

    xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkAnnotations(xmlDocPtr doc, Certificate& cert) const
{
    TestResult test{"Annotations Uniqueness", TestStatus::PASS, {}};

    // Find all <Annotations> containers in the document
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//Annotations");
    if (xpath_obj == nullptr || xpath_obj->nodesetval == nullptr)
    {
        if (xpath_obj != nullptr)
            xmlXPathFreeObject(xpath_obj);
        cert.printTestResult(test);
        return;
    }

    for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr annotations_node = xpath_obj->nodesetval->nodeTab[i];
        std::set<std::string> seen_types;

        // Check each child <Annotation> element
        for (xmlNodePtr child = annotations_node->children; child != nullptr; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            const std::string elem_name = reinterpret_cast<const char*>(child->name);
            if (elem_name == "Annotation")
            {
                auto type = getXmlAttribute(child, "type");
                if (type)
                {
                    if (seen_types.contains(*type))
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(std::format(
                            "Annotation of type '{}' (line {}): is defined multiple times within the same container.",
                            *type, child->line));
                    }
                    seen_types.insert(*type);
                }
            }
        }
    }

    xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkGenerationDateReleaseYear(const std::string& dt, std::time_t generation_time,
                                                                 TestResult& test) const
{
    checkGenerationDateReleaseYearBase(dt, generation_time, 2022, "3.0", test);
}

void Fmi3ModelDescriptionChecker::checkGuid(const std::optional<std::string>& guid_opt, Certificate& cert) const
{
    TestResult test{"Instantiation Token", TestStatus::PASS, {}};

    if (!guid_opt.has_value())
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back("instantiationToken attribute is missing.");
        cert.printTestResult(test);
        return;
    }

    if (guid_opt->empty())
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back("instantiationToken attribute is empty.");
        cert.printTestResult(test);
        return;
    }

    if (test.getStatus() != TestStatus::PASS)
        test.getMessages().emplace_back(std::format("Token: {}", *guid_opt));

    const std::string& guid = *guid_opt;
    const std::regex guid_pattern(
        R"(^(\{)?[0-9a-fA-F]{8}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{12}(\})?$)");

    if (!std::regex_match(guid, guid_pattern))
    {
        test.setStatus(TestStatus::WARNING);
        test.getMessages().emplace_back(std::format("instantiationToken '{}': does not match GUID format. While "
                                                    "allowed in FMI 3.0, using a GUID is recommended for uniqueness.",
                                                    guid));
    }

    cert.printTestResult(test);
}

ModelMetadata Fmi3ModelDescriptionChecker::extractMetadata(xmlNodePtr root) const
{
    ModelMetadata metadata;
    metadata.fmi_version = getXmlAttribute(root, "fmiVersion");
    metadata.model_name = getXmlAttribute(root, "modelName");
    metadata.guid = getXmlAttribute(root, "instantiationToken");
    metadata.model_version = getXmlAttribute(root, "version");
    metadata.author = getXmlAttribute(root, "author");
    metadata.copyright = getXmlAttribute(root, "copyright");
    metadata.license = getXmlAttribute(root, "license");
    metadata.description = getXmlAttribute(root, "description");
    metadata.generation_tool = getXmlAttribute(root, "generationTool");
    metadata.generation_date_and_time = getXmlAttribute(root, "generationDateAndTime");
    metadata.variable_naming_convention = getXmlAttribute(root, "variableNamingConvention").value_or("flat");

    return metadata;
}

void Fmi3ModelDescriptionChecker::checkUnits(xmlDocPtr doc, Certificate& cert) const
{
    TestResult test{"Unit Definitions", TestStatus::PASS, {}};

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//UnitDefinitions/Unit");
    if (xpath_obj == nullptr || xpath_obj->nodesetval == nullptr)
    {
        if (xpath_obj != nullptr)
            xmlXPathFreeObject(xpath_obj);
        cert.printTestResult(test);
        return;
    }

    xmlNodeSetPtr nodes = xpath_obj->nodesetval;
    if (nodes == nullptr)
    {
        xmlXPathFreeObject(xpath_obj);
        cert.printTestResult(test);
        return;
    }

    std::set<std::string> seen_names;

    auto checkSpecial = [&](const std::optional<std::string>& val, const std::string& attr_name,
                            const std::string& context, size_t line)
    {
        if (val && isSpecialFloat(*val))
            validateUnitSpecialFloat(test, *val, attr_name, context, line);
    };

    for (int32_t i = 0; i < nodes->nodeNr; ++i)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr unit_node = nodes->nodeTab[i];
        auto name_opt = getXmlAttribute(unit_node, "name");
        const std::string name = name_opt.value_or("unnamed");

        if (name_opt)
        {
            if (seen_names.contains(*name_opt))
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Unit '{}' (line {}): is defined multiple times.", *name_opt, unit_node->line));
            }
            seen_names.insert(*name_opt);
        }

        // FMI 3.0 factor/offset on Unit
        checkSpecial(getXmlAttribute(unit_node, "factor"), "factor", "Unit '" + name + "'", unit_node->line);
        checkSpecial(getXmlAttribute(unit_node, "offset"), "offset", "Unit '" + name + "'", unit_node->line);

        for (xmlNodePtr child = unit_node->children; child != nullptr; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            const std::string elem_name = reinterpret_cast<const char*>(child->name);

            if (elem_name == "DisplayUnit")
            {
                auto du_name = getXmlAttribute(child, "name").value_or("unnamed");
                const std::string context = std::format("Unit '{}' DisplayUnit '{}'", name, du_name);
                checkSpecial(getXmlAttribute(child, "factor"), "factor", context, child->line);
                checkSpecial(getXmlAttribute(child, "offset"), "offset", context, child->line);
            }
        }
    }

    xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

std::map<std::string, UnitDefinition> Fmi3ModelDescriptionChecker::extractUnitDefinitions(xmlDocPtr doc) const
{
    std::map<std::string, UnitDefinition> units;

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//UnitDefinitions/Unit");
    if (xpath_obj == nullptr || xpath_obj->nodesetval == nullptr)
    {
        if (xpath_obj != nullptr)
            xmlXPathFreeObject(xpath_obj);
        return units;
    }

    for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr unit_node = xpath_obj->nodesetval->nodeTab[i];
        UnitDefinition unit_def;

        unit_def.name = getXmlAttribute(unit_node, "name").value_or("");
        unit_def.sourceline = unit_node->line;

        // FMI 3.0 has factor/offset directly on Unit
        unit_def.factor = getXmlAttribute(unit_node, "factor");
        unit_def.offset = getXmlAttribute(unit_node, "offset");

        if (unit_def.name.empty())
            continue;

        for (xmlNodePtr child = unit_node->children; child != nullptr; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            const std::string elem_name = reinterpret_cast<const char*>(child->name);

            if (elem_name == "DisplayUnit")
            {
                auto display_unit_name = getXmlAttribute(child, "name");
                if (display_unit_name)
                {
                    DisplayUnit du;
                    du.name = *display_unit_name;
                    du.factor = getXmlAttribute(child, "factor");
                    du.offset = getXmlAttribute(child, "offset");
                    du.sourceline = child->line;
                    unit_def.display_units[du.name] = du;
                }
            }
        }

        units[unit_def.name] = unit_def;
    }

    xmlXPathFreeObject(xpath_obj);
    return units;
}

void Fmi3ModelDescriptionChecker::validateFmiVersionValue(const std::string& version, TestResult& test) const
{
    // FMI 3.0: must be exactly "3.0" (or follow the official FMI 3.0+ regex)
    if (version != "3.0")
    {
        static const std::regex fmi3_regex(R"(^3[.](0|[1-9][0-9]*)([.](0|[1-9][0-9]*))?(-.+)?$)");
        if (!std::regex_match(version, fmi3_regex))
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format("version '{}' is invalid.", version));
        }
        else if (version != "3.0")
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format("version '{}' is invalid (must be exactly '3.0').", version));
        }
    }
}
