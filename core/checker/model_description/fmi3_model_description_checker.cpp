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
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

void Fmi3ModelDescriptionChecker::performVersionSpecificChecks(
    xmlDocPtr doc, const std::vector<Variable>& variables,
    [[maybe_unused]] const std::map<std::string, TypeDefinition>& type_definitions,
    [[maybe_unused]] const std::map<std::string, UnitDefinition>& units, Certificate& cert)
{
    // FMI3-specific checks
    checkEnumerationVariables(variables, cert);
    checkIndependentVariable(variables, cert);
    checkDimensionReferences(variables, cert);
    checkArrayStartValues(variables, cert);
    checkClockReferences(variables, cert);
    checkClockedVariables(variables, cert);
    checkAliases(variables, cert);
    checkReinitAttribute(variables, cert);
    checkDerivativeConsistency(variables, cert);
    checkCanHandleMultipleSet(variables, cert);
    checkClockTypes(doc, cert);
    checkStructuralParameter(variables, cert);
    checkDerivativeDimensions(variables, cert);
    checkModelStructure(doc, variables, cert);
}

std::vector<Variable> Fmi3ModelDescriptionChecker::extractVariables(xmlDocPtr doc)
{
    std::vector<Variable> variables;

    // FMI3 uses direct type elements under ModelVariables
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelVariables/*");
    if (!xpath_obj)
        return variables;

    xmlNodeSetPtr nodes = xpath_obj->nodesetval;
    if (!nodes)
    {
        xmlXPathFreeObject(xpath_obj);
        return variables;
    }

    for (int32_t i = 0; i < nodes->nodeNr; ++i)
    {
        xmlNodePtr node = nodes->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
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
            std::replace(s.begin(), s.end(), ',', ' ');
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
            // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
            for (xmlNodePtr child = node->children; child; child = child->next)
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
            // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
        }

        var.unit = getXmlAttribute(node, "unit");
        var.display_unit = getXmlAttribute(node, "displayUnit");
        var.declared_type = getXmlAttribute(node, "declaredType");
        var.clocks = getXmlAttribute(node, "clocks");
        extractDimensions(node, var);
        var.sourceline = node->line;

        static auto parse_bool = [](const std::optional<std::string>& s) -> std::optional<bool>
        {
            if (!s)
                return std::nullopt;
            std::string val = *s;
            val.erase(std::remove_if(val.begin(), val.end(), ::isspace), val.end());
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

std::string Fmi3ModelDescriptionChecker::getVariableType(xmlNodePtr node)
{
    return std::string(
        reinterpret_cast<const char*>(node->name)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

void Fmi3ModelDescriptionChecker::applyDefaultInitialValues(std::vector<Variable>& variables)
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

void Fmi3ModelDescriptionChecker::checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Legal Variability", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        // FMI3: Non-Real types (Float32, Float64) cannot be continuous
        if (var.type != "Float32" && var.type != "Float64" && var.variability == "continuous")
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") is of type " + var.type +
                                    " and cannot have variability \"continuous\". Only variables of type Float32 or "
                                    "Float64 can be continuous.");
        }

        // FMI3: causality="parameter", "calculatedParameter" or "structuralParameter" must have variability="fixed" or
        // "tunable"
        if ((var.causality == "parameter" || var.causality == "calculatedParameter" ||
             var.causality == "structuralParameter") &&
            (var.variability != "fixed" && var.variability != "tunable"))
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") has causality=\"" + var.causality + "\" but variability=\"" + var.variability +
                                    "\". Parameters must be \"fixed\" or \"tunable\".");
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkClockTypes(xmlDocPtr doc, Certificate& cert)
{
    TestResult test{"Clock Type Validation", TestStatus::PASS, {}};

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//TypeDefinitions/ClockType");
    if (!xpath_obj || !xpath_obj->nodesetval)
    {
        if (xpath_obj)
            xmlXPathFreeObject(xpath_obj);
        cert.printTestResult(test);
        return;
    }

    for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
    {
        xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        auto name = getXmlAttribute(node, "name").value_or("unnamed");
        auto iv = getXmlAttribute(node, "intervalVariability").value_or("");

        // intervalDecimal required if Clock type is constant, fixed or tunable periodic Clocks.
        if (iv == "constant" || iv == "fixed" || iv == "tunable")
        {
            auto id = getXmlAttribute(node, "intervalDecimal");
            auto ic = getXmlAttribute(node, "intervalCounter");
            if (!id && !ic)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back(
                    std::format("ClockType \"{}\" (line {}) has intervalVariability='{}' but missing 'intervalDecimal' "
                                "or 'intervalCounter'.",
                                name, node->line, iv));
            }
        }
    }

    xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkRequiredStartValues(const std::vector<Variable>& variables, Certificate& cert)
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
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") must have a start value.");
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkCausalityVariabilityInitialCombinations(const std::vector<Variable>& variables,
                                                                               Certificate& cert)
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
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") has illegal combination: causality=\"" + var.causality + "\", variability=\"" +
                                    var.variability + "\", initial=\"" + initial + "\".");
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkIllegalStartValues(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Illegal Start Values", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        // Variables with initial="calculated" should not have start values
        if (var.initial == "calculated" && var.start.has_value())
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") has initial=\"calculated\" but provides a start value.");
        }

        // FMI3: Independent variables should not have start values
        if (var.causality == "independent" && var.start.has_value())
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") has causality=\"independent\" but provides a start value.");
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkMinMaxStartValues(const std::vector<Variable>& variables,
                                                         const std::map<std::string, TypeDefinition>& type_definitions,
                                                         Certificate& cert)
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
void Fmi3ModelDescriptionChecker::checkEnumerationVariables(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Enumeration Variable Type", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        if (var.type == "Enumeration" && !var.declared_type.has_value())
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") is of type Enumeration and must have a declaredType attribute.");
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkIndependentVariable(const std::vector<Variable>& variables, Certificate& cert)
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
                test.status = TestStatus::FAIL;
                test.messages.push_back("Independent variable \"" + var.name + "\" (line " +
                                        std::to_string(var.sourceline) +
                                        ") must be of floating point type (Float32 or Float64).");
            }

            // FMI3: Check for illegal initial attribute
            if (!var.initial.empty())
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Independent variable \"" + var.name + "\" (line " +
                                        std::to_string(var.sourceline) + ") must not have an initial attribute.");
            }

            // FMI3: Check for illegal start attribute
            if (var.start.has_value())
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Independent variable \"" + var.name + "\" (line " +
                                        std::to_string(var.sourceline) + ") must not have a start attribute.");
            }
        }
    }

    // FMI3: Exactly one independent variable required
    if (independent_count != 1)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Exactly one independent variable must be defined, found " +
                                std::to_string(independent_count) + ".");
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkDerivativeConsistency(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Derivative Consistency", TestStatus::PASS, {}};

    std::map<uint32_t, const Variable*> vr_map;
    for (const auto& var : variables)
        if (var.value_reference.has_value())
            vr_map[*var.value_reference] = &var;

    for (const auto& var : variables)
    {
        if (var.derivative_of.has_value())
        {
            // 1. Variability of derivative must be continuous
            if (var.variability != "continuous")
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") is a derivative and must have variability=\"continuous\".");
            }

            // 2. Must be Float32 or Float64
            if (var.type != "Float32" && var.type != "Float64")
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") has 'derivative' attribute but is of type " + var.type +
                                        " (must be Float32 or Float64).");
            }

            const uint32_t ref_vr = *var.derivative_of;
            auto it = vr_map.find(ref_vr);

            if (it == vr_map.end())
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") has derivative attribute referencing value reference " +
                                        std::to_string(ref_vr) + " which does not exist.");
            }
            else
            {
                const Variable* state_var = it->second;
                // 3. State variable must have variability="continuous"
                if (state_var->variability != "continuous")
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                            ") is derivative of \"" + state_var->name + "\" (line " +
                                            std::to_string(state_var->sourceline) + ") which has variability \"" +
                                            state_var->variability +
                                            "\". Continuous-time states must have variability=\"continuous\".");
                }
            }
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkCanHandleMultipleSet(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"CanHandleMultipleSetPerTimeInstant", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        if (var.can_handle_multiple_set.has_value() && var.causality != "input")
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") has 'canHandleMultipleSetPerTimeInstant' attribute but causality is '" +
                                    var.causality + "' (must be 'input').");
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkReinitAttribute(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Reinit Attribute", TestStatus::PASS, {}};

    // Continuous-time states are variables referenced by 'derivative' attribute of some other variable
    std::set<uint32_t> state_vrs;
    for (const auto& var : variables)
        if (var.derivative_of.has_value())
            state_vrs.insert(*var.derivative_of);

    for (const auto& var : variables)
    {
        if (var.reinit.has_value())
        {
            // FMI3: reinit may only be present for continuous-time states
            if (!var.value_reference.has_value() || !state_vrs.contains(*var.value_reference))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") has 'reinit' attribute but is not a continuous-time state.");
            }

            // 2. Must be Float32 or Float64 (only these can be continuous-time states anyway)
            if (var.type != "Float32" && var.type != "Float64")
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") has 'reinit' attribute but is of type " + var.type +
                                        " (must be Float32 or Float64).");
            }
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkAliases(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Alias Variables", TestStatus::PASS, {}};

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
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variables sharing VR " + std::to_string(vr) + " must have the same type. \"" +
                                        var->name + "\" is " + var->type + " but \"" + first->name + "\" is " +
                                        first->type + ".");
            }

            // 2. Same unit and displayUnit
            if (var->unit != first->unit)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variables sharing VR " + std::to_string(vr) + " must have the same unit. \"" +
                                        var->name + "\" has unit \"" + var->unit.value_or("(none)") + "\" but \"" +
                                        first->name + "\" has unit \"" + first->unit.value_or("(none)") + "\".");
            }
            if (var->display_unit != first->display_unit)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variables sharing VR " + std::to_string(vr) +
                                        " must have the same displayUnit. \"" + var->name + "\" has displayUnit \"" +
                                        var->display_unit.value_or("(none)") + "\" but \"" + first->name +
                                        "\" has displayUnit \"" + first->display_unit.value_or("(none)") + "\".");
            }

            // 3. Same relativeQuantity
            if (var->relative_quantity != first->relative_quantity)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variables sharing VR " + std::to_string(vr) +
                                        " must have the same relativeQuantity attribute.");
            }

            // 4. Same variability
            if (var->variability != first->variability)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back(
                    "Variables sharing VR " + std::to_string(vr) + " must have the same variability. \"" + var->name +
                    "\" is " + var->variability + " but \"" + first->name + "\" is " + first->variability + ".");
            }

            // 5. Same dimensions
            if (!compareDimensions(*var, *first))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variables sharing VR " + std::to_string(vr) +
                                        " must have the same dimensions. Variable \"" + var->name +
                                        "\" dimensions do not match \"" + first->name + "\".");
            }
        }

        // 5. Causality: At most one variable in an alias set can be non-local.
        std::vector<const Variable*> non_local;
        for (const auto* v : alias_set)
            if (v->causality != "local")
                non_local.push_back(v);

        if (non_local.size() > 1)
        {
            test.status = TestStatus::FAIL;
            std::string msg = "Alias set for VR " + std::to_string(vr) +
                              " has multiple variables with causality other than 'local': ";
            for (size_t i = 0; i < non_local.size(); ++i)
                msg += (i > 0 ? ", " : "") + non_local[i]->name;
            msg +=
                ". At most one variable in an alias set can be non-local (parameter, input, output, or independent).";
            test.messages.push_back(msg);
        }

        // 6. Constant variables: must have identical start values if they are aliased
        if (first->variability == "constant")
        {
            for (const auto* var : alias_set)
            {
                if (var->start != first->start)
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Aliased constant variables \"" + var->name + "\" and \"" + first->name +
                                            "\" (sharing VR " + std::to_string(vr) +
                                            ") have different start values ('" + var->start.value_or("") + "' vs '" +
                                            first->start.value_or("") + "').");
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
            test.status = TestStatus::FAIL;
            std::string msg = "Alias set for VR " + std::to_string(vr) +
                              " has multiple non-constant variables with a start attribute: ";
            for (size_t i = 0; i < non_constant_with_start.size(); ++i)
                msg += (i > 0 ? ", " : "") + non_constant_with_start[i]->name;
            msg += ". At most one variable in an alias set (where at least one is not constant) can have a start "
                   "attribute.";
            test.messages.push_back(msg);
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkStructuralParameter(const std::vector<Variable>& variables, Certificate& cert)
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
                test.status = TestStatus::FAIL;
                test.messages.push_back("Structural parameter \"" + var.name + "\" (line " +
                                        std::to_string(var.sourceline) + ") must be of type UInt64, found " + var.type +
                                        ".");
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
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                            ") references value reference " + std::to_string(vr) +
                                            " in <Dimension> which is not a structural parameter.");
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
                                test.status = TestStatus::FAIL;
                                test.messages.push_back("Structural parameter \"" + sp->name + "\" (line " +
                                                        std::to_string(sp->sourceline) +
                                                        ") is referenced in <Dimension> and must have start > 0.");
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
                                                      Certificate& cert)
{
    validateOutputs(doc, variables, cert);
    validateDerivatives(doc, variables, cert);
    validateClockedStates(doc, variables, cert);
    validateInitialUnknowns(doc, variables, cert);
    validateEventIndicators(doc, variables, cert);
    checkVariableDependencies(doc, variables, cert);
}

void Fmi3ModelDescriptionChecker::validateOutputs(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                  Certificate& cert)
{
    TestResult test{"ModelStructure Outputs", TestStatus::PASS, {}};

    // Get expected outputs (all variables with causality="output")
    std::set<uint32_t> expected_vrs;
    std::map<uint32_t, std::string> vr_to_name;
    for (const auto& var : variables)
    {
        if (var.causality == "output" && var.value_reference.has_value())
        {
            expected_vrs.insert(*var.value_reference);
            vr_to_name[*var.value_reference] = var.name;
        }
    }

    // FMI3: Get actual outputs from ModelStructure/Output (using valueReference attribute)
    std::set<uint32_t> actual_vrs;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/Output");

    if (xpath_obj && xpath_obj->nodesetval)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            xmlNodePtr node =
                xpath_obj->nodesetval->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            auto vr_str = getXmlAttribute(node, "valueReference");

            if (vr_str.has_value())
            {
                const auto vr_opt = parseNumber<uint32_t>(*vr_str);
                if (!vr_opt)
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("ModelStructure/Output " + std::to_string(i + 1) +
                                            " has invalid valueReference \"" + *vr_str + "\".");
                    continue;
                }
                const uint32_t vr = *vr_opt;

                if (actual_vrs.contains(vr))
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Value reference " + std::to_string(vr) +
                                            " is listed multiple times in ModelStructure/Output.");
                }
                actual_vrs.insert(vr);

                // Check if it's an output
                bool is_output = false;
                for (const auto& var : variables)
                {
                    if (var.value_reference.has_value() && *var.value_reference == vr)
                    {
                        if (var.causality == "output")
                            is_output = true;
                        else
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Variable \"" + var.name + "\" (line " +
                                                    std::to_string(var.sourceline) +
                                                    ") listed in ModelStructure/Output but does not have "
                                                    "causality=\"output\".");
                        }
                    }
                }

                if (!is_output && test.status != TestStatus::FAIL)
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Value reference " + std::to_string(vr) +
                                            " listed in ModelStructure/Output does not correspond to any output "
                                            "variable.");
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    if (expected_vrs != actual_vrs)
    {
        test.status = TestStatus::FAIL;
        std::vector<std::string> missing;
        std::vector<std::string> extra;

        for (const uint32_t vr : expected_vrs)
            if (!actual_vrs.contains(vr))
                missing.push_back(vr_to_name[vr] + " (VR " + std::to_string(vr) + ")");

        for (const uint32_t vr : actual_vrs)
            if (!expected_vrs.contains(vr))
                extra.push_back("VR " + std::to_string(vr));

        if (!missing.empty())
        {
            std::string msg =
                "The following variables with causality=\"output\" are missing from ModelStructure/Output: ";
            for (size_t i = 0; i < missing.size(); ++i)
                msg += (i > 0 ? ", " : "") + missing[i];
            msg += ".";
            test.messages.push_back(msg);
        }

        if (!extra.empty())
        {
            std::string msg = "The following variables in ModelStructure/Output do not have causality=\"output\": ";
            for (size_t i = 0; i < extra.size(); ++i)
                msg += (i > 0 ? ", " : "") + extra[i];
            msg += ".";
            test.messages.push_back(msg);
        }

        test.messages.push_back(
            "ModelStructure/Output must have exactly one entry for each variable with causality=\"output\".");
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::validateClockedStates(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                        Certificate& cert)
{
    TestResult test{"ModelStructure Clocked States", TestStatus::PASS, {}};

    // Get expected clocked states: variables with causality="local" or "output" and has clocks attribute
    std::set<uint32_t> expected_vrs;
    std::map<uint32_t, const Variable*> vr_to_var;
    for (const auto& var : variables)
    {
        if (var.value_reference.has_value())
            vr_to_var[*var.value_reference] = &var;

        if ((var.causality == "local" || var.causality == "output") && var.clocks.has_value() && !var.clocks->empty())
        {
            if (var.value_reference.has_value())
                expected_vrs.insert(*var.value_reference);
        }
    }

    // FMI3: Get actual clocked states from ModelStructure/ClockedState
    std::set<uint32_t> actual_vrs;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/ClockedState");

    if (xpath_obj && xpath_obj->nodesetval)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            xmlNodePtr node =
                xpath_obj->nodesetval->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            auto vr_str = getXmlAttribute(node, "valueReference");

            if (vr_str.has_value())
            {
                const auto vr_opt = parseNumber<uint32_t>(*vr_str);
                if (!vr_opt)
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("ModelStructure/ClockedState " + std::to_string(i + 1) +
                                            " has invalid valueReference \"" + *vr_str + "\".");
                    continue;
                }
                const uint32_t vr = *vr_opt;

                if (actual_vrs.contains(vr))
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Value reference " + std::to_string(vr) +
                                            " is listed multiple times in ModelStructure/ClockedState.");
                }
                actual_vrs.insert(vr);

                auto it = vr_to_var.find(vr);
                if (it != vr_to_var.end())
                {
                    const Variable& var = *it->second;

                    if (var.variability != "discrete")
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("Clocked state variable \"" + var.name + "\" (line " +
                                                std::to_string(var.sourceline) +
                                                ") must have variability=\"discrete\".");
                    }

                    if (!var.clocks.has_value() || var.clocks->empty())
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("Clocked state variable \"" + var.name + "\" (line " +
                                                std::to_string(var.sourceline) +
                                                ") must have the \"clocks\" attribute.");
                    }

                    if (var.type == "Clock")
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("Clocked state variable \"" + var.name + "\" (line " +
                                                std::to_string(var.sourceline) + ") must not be of type Clock.");
                    }

                    auto prev_str = getXmlAttribute(node, "previous");
                    if (!prev_str.has_value())
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("ModelStructure/ClockedState for variable \"" + var.name +
                                                "\" must have the \"previous\" attribute.");
                    }
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    if (expected_vrs != actual_vrs)
    {
        test.status = TestStatus::FAIL;
        std::vector<std::string> missing;
        for (const uint32_t vr : expected_vrs)
            if (!actual_vrs.contains(vr))
                missing.push_back(vr_to_var[vr]->name + " (VR " + std::to_string(vr) + ")");

        if (!missing.empty())
        {
            std::string msg = "The following clocked states are missing from ModelStructure/ClockedState: ";
            for (size_t i = 0; i < missing.size(); ++i)
                msg += (i > 0 ? ", " : "") + missing[i];
            msg += ".";
            test.messages.push_back(msg);
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::validateDerivatives(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                      Certificate& cert)
{
    TestResult test{"ModelStructure Derivatives", TestStatus::PASS, {}};

    // Build map of variables that are derivatives
    std::set<uint32_t> expected_vrs;
    std::map<uint32_t, std::string> vr_to_name;
    for (const auto& var : variables)
    {
        if (var.derivative_of.has_value() && var.value_reference.has_value())
        {
            expected_vrs.insert(*var.value_reference);
            vr_to_name[*var.value_reference] = var.name;
        }
    }

    // FMI3: Check ContinuousStateDerivative entries (using valueReference attribute)
    std::set<uint32_t> actual_vrs;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/ContinuousStateDerivative");

    if (xpath_obj && xpath_obj->nodesetval)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            xmlNodePtr node =
                xpath_obj->nodesetval->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            auto vr_str = getXmlAttribute(node, "valueReference");

            if (vr_str.has_value())
            {
                const auto vr_opt = parseNumber<uint32_t>(*vr_str);
                if (!vr_opt)
                    continue;
                const uint32_t vr = *vr_opt;

                if (actual_vrs.contains(vr))
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Value reference " + std::to_string(vr) +
                                            " is listed multiple times in ModelStructure/ContinuousStateDerivative.");
                }
                actual_vrs.insert(vr);

                if (!expected_vrs.contains(vr))
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Value reference " + std::to_string(vr) +
                                            " listed in ModelStructure/ContinuousStateDerivative does not have the "
                                            "\"derivative\" attribute.");
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    if (expected_vrs != actual_vrs)
    {
        test.status = TestStatus::FAIL;
        std::vector<std::string> missing;
        std::vector<std::string> extra;

        for (const uint32_t vr : expected_vrs)
            if (!actual_vrs.contains(vr))
                missing.push_back(vr_to_name[vr] + " (VR " + std::to_string(vr) + ")");

        for (const uint32_t vr : actual_vrs)
            if (!expected_vrs.contains(vr))
                extra.push_back("VR " + std::to_string(vr));

        if (!missing.empty())
        {
            std::string msg = "The following variables with a \"derivative\" attribute are missing from "
                              "ModelStructure/ContinuousStateDerivative: ";
            for (size_t i = 0; i < missing.size(); ++i)
                msg += (i > 0 ? ", " : "") + missing[i];
            msg += ".";
            test.messages.push_back(msg);
        }

        if (!extra.empty())
        {
            std::string msg = "The following value references in ModelStructure/ContinuousStateDerivative do not "
                              "correspond to derivatives: ";
            for (size_t i = 0; i < extra.size(); ++i)
                msg += (i > 0 ? ", " : "") + extra[i];
            msg += ".";
            test.messages.push_back(msg);
        }

        test.messages.push_back(
            "ModelStructure/ContinuousStateDerivative must have exactly one entry for each variable that has a "
            "\"derivative\" attribute.");
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkDerivativeDimensions(const std::vector<Variable>& variables, Certificate& cert)
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
                test.status = TestStatus::FAIL;

                const std::string derivative_dims = formatDimensions(var);
                const std::string state_dims = formatDimensions(*state_var);

                test.messages.push_back(std::format(
                    "Variable \"{}\" (line {}) is derivative of \"{}\" (line {}) but has different dimensions. "
                    "Derivative dimensions: {}, State dimensions: {}.",
                    var.name, var.sourceline, state_var->name, state_var->sourceline, derivative_dims, state_dims));
            }
        }
    }

    cert.printTestResult(test);
}

// Helper function to compare dimensions between two variables
bool Fmi3ModelDescriptionChecker::compareDimensions(const Variable& var1, const Variable& var2)
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
std::string Fmi3ModelDescriptionChecker::formatDimensions(const Variable& var)
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
                                                            Certificate& cert)
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
            test.status = TestStatus::FAIL;
            test.messages.push_back(elem_name + " (VR " + std::to_string(unknown_vr) +
                                    ") has 'dependenciesKind' but 'dependencies' is missing.");
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
                    test.status = TestStatus::FAIL;
                    test.messages.push_back(elem_name + " (VR " + std::to_string(unknown_vr) +
                                            ") has different number of elements in 'dependencies' (" +
                                            std::to_string(deps.size()) + ") and 'dependenciesKind' (" +
                                            std::to_string(kinds.size()) + ").");
                }

                for (size_t i = 0; i < kinds.size(); ++i)
                {
                    const std::string& k = kinds[i];

                    // 3. 'constant' only for floating point unknowns
                    if (k == "constant" && unknown_var && unknown_var->type != "Float32" &&
                        unknown_var->type != "Float64")
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back(elem_name + " (VR " + std::to_string(unknown_vr) +
                                                ") has dependencyKind 'constant' but unknown is not a float type.");
                    }

                    // 4. 'fixed', 'tunable', 'discrete' only for floating point unknowns AND NOT for InitialUnknown
                    if (k == "fixed" || k == "tunable" || k == "discrete")
                    {
                        if (is_initial_unknown)
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back(std::format("{} (VR {}) has illegal dependencyKind '{}' (not "
                                                                "allowed for InitialUnknown).",
                                                                elem_name, unknown_vr, k));
                        }
                        else if (unknown_var && unknown_var->type != "Float32" && unknown_var->type != "Float64")
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back(std::format("{} (VR {}) has dependencyKind '{}' but unknown is not "
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
                    test.status = TestStatus::FAIL;
                    test.messages.push_back(elem_name + " (VR " + std::to_string(unknown_vr) +
                                            ") references non-existent dependency VR " + std::to_string(d_vr) + ".");
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
        if (xpath_obj && xpath_obj->nodesetval)
        {
            for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
            {
                xmlNodePtr node =
                    xpath_obj->nodesetval->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                check_deps(node, xpath.substr(xpath.find_last_of('/') + 1), is_initial);
            }
            xmlXPathFreeObject(xpath_obj);
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::validateEventIndicators(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                          Certificate& cert)
{
    TestResult test{"ModelStructure Event Indicators", TestStatus::PASS, {}};

    // FMI 3.0: ModelStructure/EventIndicator elements define the event indicators.
    // There is no numberOfEventIndicators attribute on the root element.

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/EventIndicator");

    if (xpath_obj && xpath_obj->nodesetval)
    {
        std::set<uint32_t> seen_vrs;
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            xmlNodePtr node =
                xpath_obj->nodesetval->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            auto vr_str = getXmlAttribute(node, "valueReference");

            if (vr_str.has_value())
            {
                const auto vr_opt = parseNumber<uint32_t>(*vr_str);
                if (!vr_opt)
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("ModelStructure/EventIndicator " + std::to_string(i + 1) +
                                            " has invalid valueReference \"" + *vr_str + "\".");
                    continue;
                }
                const uint32_t vr = *vr_opt;

                if (seen_vrs.contains(vr))
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Value reference " + std::to_string(vr) +
                                            " is listed multiple times in ModelStructure/EventIndicator.");
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
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Variable \"" + var.name + "\" (line " +
                                                    std::to_string(var.sourceline) +
                                                    ") is used as an event indicator but does not have "
                                                    "causality='local' or 'output'.");
                        }

                        // Only continuous variables of type Float32 and Float64 can be referenced by EventIndicator
                        if (var.type != "Float32" && var.type != "Float64")
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Variable \"" + var.name + "\" (line " +
                                                    std::to_string(var.sourceline) +
                                                    ") is used as an event indicator but is of type " + var.type +
                                                    " (must be Float32 or Float64).");
                        }

                        if (var.variability != "continuous")
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Variable \"" + var.name + "\" (line " +
                                                    std::to_string(var.sourceline) +
                                                    ") is used as an event indicator but does not have "
                                                    "variability='continuous'.");
                        }
                        break;
                    }
                }

                if (!found)
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("ModelStructure/EventIndicator references non-existent valueReference " +
                                            std::to_string(vr) + ".");
                }
            }
            else
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("ModelStructure/EventIndicator " + std::to_string(i + 1) +
                                        " is missing the mandatory 'valueReference' attribute.");
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::validateInitialUnknowns(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                          Certificate& cert)
{
    TestResult test{"ModelStructure Initial Unknowns", TestStatus::PASS, {}};

    // Build expected set of initial unknown value references (FMI3 spec)
    std::set<uint32_t> expected_vrs;
    std::map<uint32_t, std::string> vr_to_name;

    for (const auto& var : variables)
    {
        if (!var.value_reference.has_value())
            continue;

        bool is_required = false;

        // Mandatory unknowns according to FMI 3.0 spec:
        // (1) Outputs with initial="approx" or "calculated" (not clocked)
        // (2) Calculated parameters
        // (3) State derivatives with initial="approx" or "calculated"
        if ((var.causality == "output" && (var.initial == "approx" || var.initial == "calculated") &&
             !var.clocks.has_value()) ||
            (var.causality == "calculatedParameter") ||
            (var.derivative_of.has_value() && (var.initial == "approx" || var.initial == "calculated")))
        {
            is_required = true;
        }
        // (4) States with initial="approx" or "calculated"
        // Identifying states: variables referenced by 'derivative' attribute of some other variable
        else
        {
            for (const auto& other : variables)
            {
                if (other.derivative_of.has_value() && *other.derivative_of == *var.value_reference)
                {
                    if (var.initial == "approx" || var.initial == "calculated")
                    {
                        is_required = true;
                        break;
                    }
                }
            }
        }

        if (is_required)
        {
            expected_vrs.insert(*var.value_reference);
            vr_to_name[*var.value_reference] = var.name;
        }
    }

    // FMI3: Get actual initial unknowns (using valueReference attribute)
    std::set<uint32_t> actual_vrs;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/InitialUnknown");

    if (xpath_obj && xpath_obj->nodesetval)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            xmlNodePtr node =
                xpath_obj->nodesetval->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            auto vr_str = getXmlAttribute(node, "valueReference");

            if (vr_str.has_value())
            {
                if (const auto vr_opt = parseNumber<uint32_t>(*vr_str))
                {
                    const uint32_t vr = *vr_opt;
                    if (actual_vrs.contains(vr))
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("Value reference " + std::to_string(vr) +
                                                " is listed multiple times in ModelStructure/InitialUnknown.");
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

    bool mismatch = false;
    std::vector<std::string> missing_mandatory;
    for (const uint32_t vr : expected_vrs)
    {
        if (!actual_vrs.contains(vr))
        {
            missing_mandatory.push_back(vr_to_name[vr] + " (VR " + std::to_string(vr) + ")");
            mismatch = true;
        }
    }

    std::vector<std::string> extra_invalid;
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
                extra_invalid.push_back("VR " + std::to_string(vr));
                mismatch = true;
            }
        }
    }

    if (mismatch)
    {
        test.status = TestStatus::FAIL;

        if (!missing_mandatory.empty())
        {
            std::string msg = "The following mandatory variables are missing from ModelStructure/InitialUnknown: ";
            for (size_t i = 0; i < missing_mandatory.size(); ++i)
                msg += (i > 0 ? ", " : "") + missing_mandatory[i];
            msg += ".";
            test.messages.push_back(msg);
        }

        if (!extra_invalid.empty())
        {
            std::string msg = "The following variables in ModelStructure/InitialUnknown are not allowed (only "
                              "mandatory unknowns and optional clocked variables are allowed): ";
            for (size_t i = 0; i < extra_invalid.size(); ++i)
                msg += (i > 0 ? ", " : "") + extra_invalid[i];
            msg += ".";
            test.messages.push_back(msg);
        }
    }

    cert.printTestResult(test);
}

std::map<std::string, TypeDefinition> Fmi3ModelDescriptionChecker::extractTypeDefinitions(xmlDocPtr doc)
{
    std::map<std::string, TypeDefinition> type_definitions;

    // FMI3: Type definitions are direct children of TypeDefinitions (Float32Type, Float64Type, Int8Type, etc.)
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//TypeDefinitions/*");
    if (!xpath_obj)
        return type_definitions;

    xmlNodeSetPtr nodes = xpath_obj->nodesetval;
    if (!nodes)
    {
        xmlXPathFreeObject(xpath_obj);
        return type_definitions;
    }

    for (int32_t i = 0; i < nodes->nodeNr; ++i)
    {
        xmlNodePtr type_node = nodes->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        TypeDefinition type_def;

        // Get name attribute
        type_def.name = getXmlAttribute(type_node, "name").value_or("");
        type_def.sourceline = type_node->line;

        // Element name IS the type (Float32Type, Int8Type, BooleanType, etc.)
        const std::string elem_name =
            reinterpret_cast<const char*>(type_node->name); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

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

void Fmi3ModelDescriptionChecker::extractDimensions(xmlNodePtr node, Variable& var)
{
    // Look for Dimension child elements
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    for (xmlNodePtr child = node->children; child; child = child->next)
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
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
}

void Fmi3ModelDescriptionChecker::checkDimensionReferences(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Dimension References", TestStatus::PASS, {}};

    // Build a map of value_reference -> Variable for structural parameters
    std::map<uint32_t, const Variable*> structural_params_by_vr;
    for (const auto& var : variables)
        if (var.causality == "structuralParameter" && var.value_reference.has_value())
            structural_params_by_vr[*var.value_reference] = &var;

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
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                            "), Dimension " + std::to_string(i + 1) + " (line " +
                                            std::to_string(dim.sourceline) +
                                            "): must have either 'start' or 'valueReference' attribute.");
                }
                else if (has_start && has_vr)
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                            "), Dimension " + std::to_string(i + 1) + " (line " +
                                            std::to_string(dim.sourceline) +
                                            "): must have either 'start' OR 'valueReference', not both.");
                }

                // If valueReference is used, check that it points to a structural parameter
                if (has_vr)
                {
                    const uint32_t vr = *dim.value_reference;
                    auto it = structural_params_by_vr.find(vr);

                    if (it == structural_params_by_vr.end())
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("Variable \"" + var.name + "\" (line " +
                                                std::to_string(var.sourceline) + "), Dimension " +
                                                std::to_string(i + 1) + " (line " + std::to_string(dim.sourceline) +
                                                ") references value reference " + std::to_string(vr) +
                                                " which is not a structural parameter.");
                    }
                    else
                    {
                        const Variable* sp = it->second;

                        // Check that the structural parameter is of type UInt64
                        if (sp->type != "UInt64")
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Variable \"" + var.name + "\" (line " +
                                                    std::to_string(var.sourceline) + "), Dimension " +
                                                    std::to_string(i + 1) + " (line " + std::to_string(dim.sourceline) +
                                                    ") references structural parameter \"" + sp->name +
                                                    "\" which has type \"" + sp->type + "\" (expected UInt64)");
                        }

                        // Check that the structural parameter has start > 0
                        if (sp->start.has_value())
                        {
                            if (const auto start_val_opt = parseNumber<uint64_t>(*sp->start))
                            {
                                if (*start_val_opt == 0)
                                {
                                    test.status = TestStatus::FAIL;
                                    test.messages.push_back(
                                        "Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        "), Dimension " + std::to_string(i + 1) + " (line " +
                                        std::to_string(dim.sourceline) + ") references structural parameter \"" +
                                        sp->name + "\" (line " + std::to_string(sp->sourceline) +
                                        ") which has start=0 (must be > 0).");
                                }
                            }
                            else
                            {
                                test.status = TestStatus::FAIL;
                                test.messages.push_back(
                                    "Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    "), Dimension " + std::to_string(i + 1) + " (line " +
                                    std::to_string(dim.sourceline) + ") references structural parameter \"" + sp->name +
                                    "\" (line " + std::to_string(sp->sourceline) +
                                    ") which has invalid start value (not a valid UInt64).");
                            }
                        }
                        else
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back(
                                "Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                "), Dimension " + std::to_string(i + 1) + " (line " + std::to_string(dim.sourceline) +
                                ") references structural parameter \"" + sp->name + "\" (line " +
                                std::to_string(sp->sourceline) + ") which does not have a start value.");
                        }
                    }
                }

                // If start is used directly, check that it's > 0
                if (has_start && !has_vr)
                {
                    if (*dim.start == 0)
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("Variable \"" + var.name + "\" (line " +
                                                std::to_string(var.sourceline) + "), Dimension " +
                                                std::to_string(i + 1) + " (line " + std::to_string(dim.sourceline) +
                                                ") has start=0 (must be > 0).");
                    }
                }
            }
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkArrayStartValues(const std::vector<Variable>& variables, Certificate& cert)
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
                // Count the number of start values
                // In FMI3, start values for arrays can be:
                // 1. Space-separated list in the start attribute
                // 2. Comma-separated list in the start attribute
                // 3. Multiple Start child elements (for String/Binary)

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

                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                            ") is an array with dimensions [" + dim_str + "] (total size " +
                                            std::to_string(*total_size) + ") but has " +
                                            std::to_string(num_start_values) + " start value" +
                                            (num_start_values == 1 ? "" : "s") + ". Expected either " +
                                            std::to_string(*total_size) + " values or 1 scalar value (for broadcast).");
                }
            }
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkClockReferences(const std::vector<Variable>& variables, Certificate& cert)
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
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        "): Invalid clock reference '" + vr_str + "' in clocks attribute.");
                continue;
            }
        }

        // Validate each clock reference
        for (const uint32_t clock_vr : clock_refs)
        {
            // Check if a Clock is referencing itself
            if (var.type == "Clock" && var.value_reference.has_value() && *var.value_reference == clock_vr)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Clock variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        "): Clock cannot reference itself in clocks attribute.");
                continue;
            }

            // Check if the value reference exists
            auto it = vr_to_var.find(clock_vr);
            if (it == vr_to_var.end())
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        "): References non-existent clock with valueReference " +
                                        std::to_string(clock_vr));
                continue;
            }

            // Check if the referenced variable is actually a Clock
            if (it->second->type != "Clock")
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        "): References valueReference " + std::to_string(clock_vr) + " which is a " +
                                        it->second->type + ", not a Clock (variable \"" + it->second->name +
                                        "\", line " + std::to_string(it->second->sourceline) + ")");
            }
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkClockedVariables(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Check Clocked Variables", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        // Skip variables without clocks attribute
        if (!var.clocks.has_value() || var.clocks->empty())
            continue;

        // Note: Clock variables CAN have a clocks attribute (per FMI3 spec section 2.2.8.3)
        // "This also holds for clocked variables of type Clock."

        // Check causality - clocked variables must have specific causality values
        if (var.causality != "input" && var.causality != "output" && var.causality != "local")
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back(
                "Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                "): Clocked variables must have causality 'input', 'output', or 'local', but has '" + var.causality +
                "'.");
        }

        // Check variability - clocked variables must have discrete variability
        // Exception: continuous variables can be clocked if they are inputs/outputs in co-simulation
        // For simplicity, we enforce discrete for all clocked variables except for specific cases
        if (var.variability != "discrete")
        {
            // Continuous variables cannot be clocked in general
            if (var.variability == "continuous")
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        "): Continuous variables cannot have a clocks attribute. " +
                                        "Clocked variables must have variability='discrete'.");
            }
            // Constants, fixed, and tunable also cannot be clocked
            else if (var.variability == "constant" || var.variability == "fixed" || var.variability == "tunable")
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        "): Variables with variability='" + var.variability +
                                        "' cannot have a clocks attribute. " +
                                        "Clocked variables must have variability='discrete'.");
            }
        }

        // Check that independent variable is not clocked
        if (var.causality == "independent")
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    "): Independent variable cannot have a clocks attribute.");
        }

        // Check that parameters are not clocked
        if (var.causality == "parameter" || var.causality == "calculatedParameter" ||
            var.causality == "structuralParameter")
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    "): Parameters (causality='" + var.causality +
                                    "') cannot have a clocks attribute.");
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::validateVariableSpecialFloat(TestResult& /*test*/, const Variable& /*var*/,
                                                               const std::string& /*val*/,
                                                               const std::string& /*attr_name*/)
{
    // Special floats are allowed in FMI 3.0 variable values
}

void Fmi3ModelDescriptionChecker::validateDefaultExperimentSpecialFloat(TestResult& /*test*/,
                                                                        const std::string& /*val*/,
                                                                        const std::string& /*attr_name*/)
{
    // Special floats are allowed in FMI 3.0
}

void Fmi3ModelDescriptionChecker::validateUnitSpecialFloat(TestResult& /*test*/, const std::string& /*val*/,
                                                           const std::string& /*attr_name*/,
                                                           const std::string& /*context*/, size_t /*line*/)
{
    // Special floats are allowed in FMI 3.0
}

void Fmi3ModelDescriptionChecker::validateTypeDefinitionSpecialFloat(TestResult& /*test*/,
                                                                     const TypeDefinition& /*type_def*/,
                                                                     const std::string& /*val*/,
                                                                     const std::string& /*attr_name*/)
{
    // Special floats are allowed in FMI 3.0 type definitions
}

void Fmi3ModelDescriptionChecker::checkTypeDefinitions(xmlDocPtr doc, Certificate& cert)
{
    TestResult test{"Type Definitions", TestStatus::PASS, {}};

    // FMI3: Type definitions are direct children of TypeDefinitions
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//TypeDefinitions/*");
    if (!xpath_obj)
    {
        cert.printTestResult(test);
        return;
    }

    xmlNodeSetPtr nodes = xpath_obj->nodesetval;
    if (!nodes)
    {
        xmlXPathFreeObject(xpath_obj);
        cert.printTestResult(test);
        return;
    }

    std::set<std::string> seen_names;

    for (int32_t i = 0; i < nodes->nodeNr; ++i)
    {
        xmlNodePtr type_node = nodes->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        if (type_node->type != XML_ELEMENT_NODE)
            continue;

        auto name_opt = getXmlAttribute(type_node, "name");
        const std::string name = name_opt.value_or("unnamed");

        if (name_opt)
        {
            if (seen_names.contains(*name_opt))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Type definition \"" + *name_opt + "\" (line " +
                                        std::to_string(type_node->line) + ") is defined multiple times.");
            }
            seen_names.insert(*name_opt);
        }

        const std::string elem_name =
            reinterpret_cast<const char*>(type_node->name); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

        auto min_str = getXmlAttribute(type_node, "min");
        auto max_str = getXmlAttribute(type_node, "max");
        auto nominal_str = getXmlAttribute(type_node, "nominal");

        // FMI 3.0 allows special floats, so we don't need to validate them as failures here
        // (unlike FMI 2.0)

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
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("Type definition \"" + name + "\" (line " +
                                                std::to_string(type_node->line) + "): max (" + *max_str +
                                                ") must be >= min (" + *min_str + ").");
                    }
                }
                else if (elem_name.find("UInt") != std::string::npos)
                {
                    const auto min_val = parseNumber<uint64_t>(*min_str);
                    const auto max_val = parseNumber<uint64_t>(*max_str);
                    if (min_val && max_val && *max_val < *min_val)
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("Type definition \"" + name + "\" (line " +
                                                std::to_string(type_node->line) + "): max (" + *max_str +
                                                ") must be >= min (" + *min_str + ").");
                    }
                }
                else if (elem_name.find("Int") != std::string::npos || elem_name == "EnumerationType")
                {
                    const auto min_val = parseNumber<int64_t>(*min_str);
                    const auto max_val = parseNumber<int64_t>(*max_str);
                    if (min_val && max_val && *max_val < *min_val)
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("Type definition \"" + name + "\" (line " +
                                                std::to_string(type_node->line) + "): max (" + *max_str +
                                                ") must be >= min (" + *min_str + ").");
                    }
                }
            }
        }

        if (elem_name == "EnumerationType")
        {
            bool has_items = false;
            std::set<int64_t> item_values;
            std::set<std::string> item_names;

            for (xmlNodePtr item = type_node->children; item; item = item->next)
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
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Enumeration type \"" + name + "\" (line " +
                                                    std::to_string(type_node->line) + ") has multiple items named \"" +
                                                    *item_name + "\".");
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
                                test.status = TestStatus::FAIL;
                                test.messages.push_back("Enumeration type \"" + name + "\" (line " +
                                                        std::to_string(type_node->line) +
                                                        ") has multiple items with value " + *item_value_str +
                                                        ". Item values must be unique within the same enumeration.");
                            }
                            item_values.insert(val);
                        }
                    }
                }
            }

            if (!has_items)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Enumeration type \"" + name + "\" (line " + std::to_string(type_node->line) +
                                        ") must have at least one Item.");
            }
        }
    }

    xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkAnnotations(xmlDocPtr doc, Certificate& cert)
{
    TestResult test{"Annotations Uniqueness", TestStatus::PASS, {}};

    // Find all <Annotations> containers in the document
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//Annotations");
    if (!xpath_obj || !xpath_obj->nodesetval)
    {
        if (xpath_obj)
            xmlXPathFreeObject(xpath_obj);
        cert.printTestResult(test);
        return;
    }

    for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
    {
        xmlNodePtr annotations_node =
            xpath_obj->nodesetval->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::set<std::string> seen_types;

        // Check each child <Annotation> element
        for (xmlNodePtr child = annotations_node->children; child; child = child->next)
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
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("Annotation of type \"" + *type + "\" (line " +
                                                std::to_string(child->line) +
                                                ") is defined multiple times within the same container.");
                    }
                    seen_types.insert(*type);
                }
            }
        }
    }

    xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkGenerationDateReleaseYear(const std::string& dt,
                                                                 std::chrono::sys_seconds generation_time,
                                                                 TestResult& test)
{
    checkGenerationDateReleaseYearBase(dt, generation_time, 2022, "3.0", test);
}

void Fmi3ModelDescriptionChecker::checkGuid(const std::optional<std::string>& guid_opt, Certificate& cert)
{
    TestResult test{"Instantiation Token Format", TestStatus::PASS, {}};

    if (!guid_opt.has_value())
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("instantiationToken attribute is missing.");
        cert.printTestResult(test);
        return;
    }

    if (guid_opt->empty())
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("instantiationToken attribute is empty.");
        cert.printTestResult(test);
        return;
    }

    const std::string& guid = *guid_opt;
    const std::regex guid_pattern(
        R"(^(\{)?[0-9a-fA-F]{8}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{12}(\})?$)");

    if (!std::regex_match(guid, guid_pattern))
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back(
            "instantiationToken \"" + guid +
            "\" does not match GUID format. While allowed in FMI 3.0, using a GUID is recommended for uniqueness.");
    }

    cert.printTestResult(test);
}

ModelMetadata Fmi3ModelDescriptionChecker::extractMetadata(xmlNodePtr root)
{
    ModelMetadata metadata;
    metadata.fmiVersion = getXmlAttribute(root, "fmiVersion");
    metadata.modelName = getXmlAttribute(root, "modelName");
    metadata.guid = getXmlAttribute(root, "instantiationToken");
    metadata.modelVersion = getXmlAttribute(root, "version");
    metadata.author = getXmlAttribute(root, "author");
    metadata.copyright = getXmlAttribute(root, "copyright");
    metadata.license = getXmlAttribute(root, "license");
    metadata.description = getXmlAttribute(root, "description");
    metadata.generationTool = getXmlAttribute(root, "generationTool");
    metadata.generationDateAndTime = getXmlAttribute(root, "generationDateAndTime");
    metadata.variableNamingConvention = getXmlAttribute(root, "variableNamingConvention").value_or("flat");

    return metadata;
}

void Fmi3ModelDescriptionChecker::checkUnits(xmlDocPtr doc, Certificate& cert)
{
    TestResult test{"Unit Definitions", TestStatus::PASS, {}};

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//UnitDefinitions/Unit");
    if (!xpath_obj)
    {
        cert.printTestResult(test);
        return;
    }

    xmlNodeSetPtr nodes = xpath_obj->nodesetval;
    if (!nodes)
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
        xmlNodePtr unit_node = nodes->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        auto name_opt = getXmlAttribute(unit_node, "name");
        const std::string name = name_opt.value_or("unnamed");

        if (name_opt)
        {
            if (seen_names.contains(*name_opt))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Unit \"" + *name_opt + "\" (line " + std::to_string(unit_node->line) +
                                        ") is defined multiple times.");
            }
            seen_names.insert(*name_opt);
        }

        // FMI 3.0 factor/offset on Unit
        checkSpecial(getXmlAttribute(unit_node, "factor"), "factor", "Unit \"" + name + "\"", unit_node->line);
        checkSpecial(getXmlAttribute(unit_node, "offset"), "offset", "Unit \"" + name + "\"", unit_node->line);

        for (xmlNodePtr child = unit_node->children; child; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            const std::string elem_name =
                reinterpret_cast<const char*>(child->name); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            if (elem_name == "DisplayUnit")
            {
                auto du_name = getXmlAttribute(child, "name").value_or("unnamed");
                const std::string context = std::format("Unit \"{}\" DisplayUnit \"{}\"", name, du_name);
                checkSpecial(getXmlAttribute(child, "factor"), "factor", context, child->line);
                checkSpecial(getXmlAttribute(child, "offset"), "offset", context, child->line);
            }
        }
    }

    xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

std::map<std::string, UnitDefinition> Fmi3ModelDescriptionChecker::extractUnitDefinitions(xmlDocPtr doc)
{
    std::map<std::string, UnitDefinition> units;

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//UnitDefinitions/Unit");
    if (!xpath_obj)
        return units;

    xmlNodeSetPtr nodes = xpath_obj->nodesetval;
    if (!nodes)
    {
        xmlXPathFreeObject(xpath_obj);
        return units;
    }

    for (int32_t i = 0; i < nodes->nodeNr; ++i)
    {
        xmlNodePtr unit_node = nodes->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        UnitDefinition unit_def;

        unit_def.name = getXmlAttribute(unit_node, "name").value_or("");
        unit_def.sourceline = unit_node->line;

        // FMI 3.0 has factor/offset directly on Unit
        unit_def.factor = getXmlAttribute(unit_node, "factor");
        unit_def.offset = getXmlAttribute(unit_node, "offset");

        if (unit_def.name.empty())
            continue;

        for (xmlNodePtr child = unit_node->children; child; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            const std::string elem_name =
                reinterpret_cast<const char*>(child->name); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

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

void Fmi3ModelDescriptionChecker::validateFmiVersionValue(const std::string& version, TestResult& test)
{
    // FMI 3.0: must be exactly "3.0" (or follow the official FMI 3.0+ regex)
    // The user requested that for 1.0, 2.0, and 3.0, only "1.0", "2.0", and "3.0" are allowed.
    if (version != "3.0")
    {
        static const std::regex fmi3_regex(R"(^3[.](0|[1-9][0-9]*)([.](0|[1-9][0-9]*))?(-.+)?$)");
        if (!std::regex_match(version, fmi3_regex))
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("version \"" + version + "\" is invalid.");
        }
        else if (version != "3.0")
        {
            // If it matches the regex but is not "3.0", we still follow the user's rule
            test.status = TestStatus::FAIL;
            test.messages.push_back("version \"" + version + "\" is invalid (must be exactly \"3.0\").");
        }
    }
}
