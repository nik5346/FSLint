#include "fmi2_model_description_checker.h"

#include "model_description_checker.h"

#include "certificate.h"

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

void Fmi2ModelDescriptionChecker::performVersionSpecificChecks(
    xmlDocPtr doc, const std::vector<Variable>& variables,
    const std::map<std::string, TypeDefinition>& type_definitions,
    [[maybe_unused]] const std::map<std::string, UnitDefinition>& units, Certificate& cert) const
{
    // Enumeration variables must have a declaredType
    checkEnumerationVariables(variables, cert);

    // Check Alias variables
    checkAliases(variables, type_definitions, cert);

    // Check Independent variable
    checkIndependentVariable(variables, cert);

    // Check reinit attribute
    checkReinitAttribute(doc, variables, cert);

    // Check MultipleSetPerTimeInstant attribute
    checkMultipleSetAttribute(doc, variables, cert);

    // Check continuous-time states and derivatives
    checkContinuousStatesAndDerivatives(doc, variables, cert);

    // Check SourceFiles semantic validation (existence of listed files)
    checkSourceFilesSemantic(doc, cert);

    // Run FMI2-specific model structure checks (should be last)
    checkModelStructure(doc, variables, cert);
}

void Fmi2ModelDescriptionChecker::checkEnumerationVariables(const std::vector<Variable>& variables,
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

void Fmi2ModelDescriptionChecker::checkReinitAttribute(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                       Certificate& cert) const
{
    TestResult test{"Reinit Attribute", TestStatus::PASS, {}};

    const bool has_me = !extractModelIdentifiers(doc, {"ModelExchange"}).empty();
    const bool has_cs = !extractModelIdentifiers(doc, {"CoSimulation"}).empty();

    std::set<uint32_t> state_indices;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/Derivatives/Unknown");

    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            const auto index_str = getXmlAttribute(node, "index");

            if (index_str.has_value())
            {
                if (const auto index_opt = parseNumber<size_t>(*index_str))
                {
                    const size_t index = *index_opt;
                    if (index > 0 && index <= variables.size())
                    {
                        const auto& var = variables[index - 1];
                        if (var.derivative_of.has_value())
                            state_indices.insert(*var.derivative_of);
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
            if (!state_indices.contains(var.index))
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Variable '{}' (line {}) has 'reinit' attribute but is not a continuous-time state.",
                                var.name, var.sourceline));
            }

            if (!has_me && has_cs)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "Variable '{}' (line {}) has 'reinit' attribute which is not allowed for Co-Simulation only "
                    "FMUs.",
                    var.name, var.sourceline));
            }
        }
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::checkMultipleSetAttribute(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                            Certificate& cert) const
{
    TestResult test{"Multiple Set Attribute", TestStatus::PASS, {}};

    const bool has_me = !extractModelIdentifiers(doc, {"ModelExchange"}).empty();
    const bool has_cs = !extractModelIdentifiers(doc, {"CoSimulation"}).empty();

    for (const auto& var : variables)
    {
        if (var.can_handle_multiple_set.has_value())
        {
            if (var.causality != "input")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Variable '{}' (line {}) has 'canHandleMultipleSetPerTimeInstant' but causality is "
                                "'{}' (expected 'input').",
                                var.name, var.sourceline, var.causality));
            }

            if (!has_me && has_cs)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Variable '{}' (line {}) has 'canHandleMultipleSetPerTimeInstant' which is not allowed "
                                "for Co-Simulation only FMUs.",
                                var.name, var.sourceline));
            }
        }
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::checkContinuousStatesAndDerivatives(xmlDocPtr doc,
                                                                      const std::vector<Variable>& variables,
                                                                      Certificate& cert) const
{
    TestResult test{"Continuous-time States and Derivatives", TestStatus::PASS, {}};

    std::set<uint32_t> state_indices;
    std::set<uint32_t> derivative_indices;
    std::map<uint32_t, const Variable*> index_map;

    for (const auto& var : variables)
        index_map[var.index] = &var;

    // Parse ModelStructure/Derivatives to identify "active" states and derivatives
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/Derivatives/Unknown");

    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            const auto index_str = getXmlAttribute(node, "index");

            if (index_str.has_value())
            {
                if (const auto index_opt = parseNumber<size_t>(*index_str))
                {
                    const size_t index = *index_opt;
                    if (index > 0 && index <= variables.size())
                    {
                        const auto& var = variables[index - 1];
                        derivative_indices.insert(var.index);
                        if (var.derivative_of.has_value())
                            state_indices.insert(*var.derivative_of);
                    }
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    for (const auto& var : variables)
    {
        // Check active states
        if (state_indices.contains(var.index))
        {
            if (var.causality != "local" && var.causality != "output")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Continuous-time state '{}' (line {}) must have causality 'local' or 'output'.",
                                var.name, var.sourceline));
            }

            if (var.variability != "continuous")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Continuous-time state '{}' (line {}) must have variability='continuous'.", var.name,
                                var.sourceline));
            }
        }

        // Check derivatives
        if (var.derivative_of.has_value())
        {
            // The variability=continuous and type=Real checks on the derivative variable itself
            // should always apply if the derivative attribute is present (since only continuous Reals
            // can carry it).
            if (var.variability != "continuous")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Variable '{}' (line {}) is a derivative and must have variability='continuous'.",
                                var.name, var.sourceline));
            }

            if (var.type != "Real")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("State derivative '{}' (line {}) must be of type 'Real'.", var.name, var.sourceline));
            }

            // Checks on the state variable itself only apply if the derivative is listed in ModelStructure
            if (derivative_indices.contains(var.index))
            {
                const uint32_t ref_index = *var.derivative_of;
                auto it = index_map.find(ref_index);

                if (it == index_map.end())
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format(
                        "Variable '{}' (line {}) has derivative attribute referencing index {} which does not exist.",
                        var.name, var.sourceline, ref_index));
                }
                else
                {
                    const Variable* state_var = it->second;
                    if (state_var->variability != "continuous")
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(
                            std::format("Variable '{}' (line {}) is derivative of '{}' which has variability '{}'. "
                                        "Continuous-time states must have variability='continuous'.",
                                        var.name, var.sourceline, state_var->name, state_var->variability));
                    }
                }
            }
        }

        // Type check for active states
        if (state_indices.contains(var.index))
        {
            if (var.type != "Real")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "Continuous-time state '{}' (line {}) must be of type 'Real'.", var.name, var.sourceline));
            }
        }
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::checkIndependentVariable(const std::vector<Variable>& variables,
                                                           Certificate& cert) const
{
    TestResult test{"Independent Variable", TestStatus::PASS, {}};

    const Variable* independent_var = nullptr;

    for (const auto& var : variables)
    {
        if (var.causality == "independent")
        {
            if (independent_var == nullptr)
                independent_var = &var;
            else
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("At most one ScalarVariable can be defined as 'independent'. Found multiple: {})",
                                independent_var->name + ", " + var.name));
            }
        }
    }

    if (independent_var != nullptr)
    {
        if (independent_var->type != "Real")
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Independent variable '{}' must be of type 'Real'.", independent_var->name));
        }

        if (independent_var->start.has_value())
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format(
                "Independent variable '{}' is not allowed to have a 'start' attribute.", independent_var->name));
        }

        if (!independent_var->initial.empty())
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Independent variable '{}' is not allowed to define 'initial'.", independent_var->name));
        }

        if (independent_var->variability != "continuous")
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Independent variable '{}' must have variability='continuous'.", independent_var->name));
        }
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::checkAliases(const std::vector<Variable>& variables,
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

    // Group variables by base type and valueReference
    const auto get_base_type = [](const std::string& type) -> std::string
    {
        if (type == "Integer" || type == "Enumeration")
            return "Integer/Enumeration";
        return type;
    };

    std::map<std::pair<std::string, uint32_t>, std::vector<const Variable*>> alias_sets;
    for (const auto& var : variables)
        if (var.value_reference.has_value())
            alias_sets[{get_base_type(var.type), *var.value_reference}].push_back(&var);

    for (const auto& [key, alias_set] : alias_sets)
    {
        if (alias_set.size() <= 1)
            continue;

        const bool has_non_constant =
            std::ranges::any_of(alias_set, [](const Variable* v) { return v->variability != "constant"; });

        const Variable* first_constant = nullptr;

        auto can_be_set = [](const Variable* v) -> bool
        {
            if (v->variability == "constant")
                return false;
            if (v->causality == "input")
                return true;
            if (v->causality == "parameter")
                return true; // fixed or tunable
            if (v->causality == "independent")
                return true; // Set via fmi2SetTime
            if (v->initial == "exact" || v->initial == "approx")
                return true;
            return false;
        };

        std::vector<const Variable*> settable_vars;

        for (const auto* var : alias_set)
        {
            if (var->variability == "constant")
            {
                if (first_constant == nullptr)
                    first_constant = var;
            }

            if (can_be_set(var))
                settable_vars.push_back(var);
        }

        // Rule 1: Cannot both be set with fmi2SetXXX
        if (settable_vars.size() > 1)
        {
            test.setStatus(TestStatus::FAIL);
            std::string vars;
            for (size_t i = 0; i < settable_vars.size(); ++i)
                vars += (i > 0 ? ", " : "") + std::format("'{}'", settable_vars[i]->name);

            test.getMessages().emplace_back(
                std::format("All variables in an alias set (VR {}) must have at most one variable that can be set with "
                            "fmi2SetXXX. Found: {}.",
                            key.second, vars));
        }

        // Rule 2: At most one variable of the same alias set of variables with variability != "constant" can have a
        // start attribute
        if (has_non_constant)
        {
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
                    key.second, vars));
            }
        }

        // Rule 3: A variable with variability="constant" can only be aliased to another variable with
        // variability="constant"
        if (first_constant != nullptr)
        {
            for (const auto* var : alias_set)
            {
                if (var->variability != "constant")
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format(
                        "All variables in an alias set (VR {}) must have the same variability. Variable '{}' is "
                        "{} but '{}' is constant. Constants can only be aliased to other constants.",
                        key.second, var->name, var->variability, first_constant->name));
                }
                else
                {
                    // start values must be identical
                    if (var->start != first_constant->start)
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(std::format(
                            "All variables in an alias set (VR {}) must have the same start values if they are "
                            "constant. Variable '{}' has start='{}' but '{}' has start='{}'.",
                            key.second, var->name, var->start.value_or("(none)"), first_constant->name,
                            first_constant->start.value_or("(none)")));
                    }
                }
            }
        }

        // Rule 4: All variables in the same alias set must have the same unit
        const Variable* first_var = alias_set[0];
        const auto first_unit = resolve_unit(first_var);
        for (size_t i = 1; i < alias_set.size(); ++i)
        {
            const Variable* var = alias_set[i];
            const auto var_unit = resolve_unit(var);
            if (var_unit != first_unit)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("All variables in an alias set (VR {}) must have the same unit. Variable '{}' has "
                                "unit '{}' but '{}' has unit '{}'.",
                                key.second, var->name, var_unit.value_or("(none)"), first_var->name,
                                first_unit.value_or("(none)")));
            }
        }
    }

    cert.printTestResult(test);
}

std::vector<Variable> Fmi2ModelDescriptionChecker::extractVariables(xmlDocPtr doc) const
{
    std::vector<Variable> variables;

    // FMI2 uses ModelVariables/ScalarVariable structure
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelVariables/ScalarVariable");
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
        xmlNodePtr scalar_var_node = nodes->nodeTab[i];
        Variable var;

        // Get attributes from ScalarVariable element
        var.name = getXmlAttribute(scalar_var_node, "name").value_or("");
        var.causality = getXmlAttribute(scalar_var_node, "causality").value_or("local");
        var.variability = getXmlAttribute(scalar_var_node, "variability").value_or("");
        var.initial = getXmlAttribute(scalar_var_node, "initial").value_or("");
        var.declared_type = getXmlAttribute(scalar_var_node, "declaredType");
        var.sourceline = scalar_var_node->line;
        var.index = static_cast<uint32_t>(i + 1);

        auto multi_set = getXmlAttribute(scalar_var_node, "canHandleMultipleSetPerTimeInstant");
        if (multi_set)
            var.can_handle_multiple_set = (*multi_set == "true");

        auto vr = getXmlAttribute(scalar_var_node, "valueReference");
        if (vr.has_value())
            var.value_reference = parseNumber<uint32_t>(*vr);

        // FMI2: The type element (Real, Integer, Boolean, String, Enumeration) is a child of ScalarVariable
        for (xmlNodePtr child = scalar_var_node->children; child != nullptr; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            const std::string elem_name = reinterpret_cast<const char*>(child->name);

            if (elem_name == "Real" || elem_name == "Integer" || elem_name == "Boolean" || elem_name == "String" ||
                elem_name == "Enumeration")
            {
                var.type = elem_name;

                // Get type-specific attributes
                if (elem_name == "Real")
                {
                    auto rel_q = getXmlAttribute(child, "relativeQuantity");
                    if (rel_q)
                        var.relative_quantity = (*rel_q == "true");
                }
                var.start = getXmlAttribute(child, "start");
                var.min = getXmlAttribute(child, "min");
                var.max = getXmlAttribute(child, "max");
                var.nominal = getXmlAttribute(child, "nominal");
                var.unit = getXmlAttribute(child, "unit");
                var.display_unit = getXmlAttribute(child, "displayUnit");

                if (!var.declared_type.has_value())
                    var.declared_type = getXmlAttribute(child, "declaredType");

                // FMI2: derivative and reinit attributes are on the Real element
                if (elem_name == "Real")
                {
                    auto der = getXmlAttribute(child, "derivative");
                    if (der.has_value())
                        var.derivative_of = parseNumber<uint32_t>(*der);

                    auto ri = getXmlAttribute(child, "reinit");
                    if (ri)
                        var.reinit = (*ri == "true");
                }

                break;
            }
        }

        // Apply default variability if not specified (FMI2-specific rules)
        if (var.variability.empty())
        {
            if (var.type == "Real" && !(var.causality == "parameter" || var.causality == "calculatedParameter"))
            {
                var.variability = "continuous";
            }
            else if ((var.causality == "input" || var.causality == "output" || var.causality == "local") &&
                     var.type != "Real")
            {
                var.variability = "discrete";
            }
        }

        variables.push_back(var);
    }

    xmlXPathFreeObject(xpath_obj);
    return variables;
}

void Fmi2ModelDescriptionChecker::applyDefaultInitialValues(std::vector<Variable>& variables) const
{
    for (auto& var : variables)
    {
        if (!var.initial.empty())
            continue;

        // FMI2: initial NOT ALLOWED for input or independent - keep empty
        if (var.causality == "input" || var.causality == "independent")
        {
            var.initial = "";
            continue;
        }

        // Apply defaults based on FMI2 Table 4.7.1
        if (var.causality == "parameter")
        {
            if (var.variability == "fixed" || var.variability == "tunable")
                var.initial = "exact";
        }
        else if (var.causality == "calculatedParameter")
        {
            if (var.variability == "fixed" || var.variability == "tunable")
                var.initial = "calculated";
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

void Fmi2ModelDescriptionChecker::checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert) const
{
    TestResult test{"Legal Variability", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        // FMI2: Only Real types can be continuous
        if (var.type != "Real" && var.variability == "continuous")
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Variable '{}' (line {}) is of type {} and cannot have variability 'continuous'.", var.name,
                            var.sourceline, var.type));
        }

        // FMI2: causality="parameter" or "calculatedParameter" must have variability="fixed" or "tunable"
        if ((var.causality == "parameter" || var.causality == "calculatedParameter") &&
            (var.variability != "fixed" && var.variability != "tunable"))
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format("Variable '{}' (line {}) has causality='{}' but "
                                                        "variability='{}'. Parameters must be 'fixed' or 'tunable'.",
                                                        var.name, var.sourceline, var.causality, var.variability));
            ;
        }
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::validateFmiVersionValue(const std::string& version, TestResult& test) const
{
    // FMI 2.0: must be exactly "2.0"
    if (version != "2.0")
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back(std::format("version '{}' is invalid (must be exactly '2.0').", version));
    }
}

void Fmi2ModelDescriptionChecker::checkRequiredStartValues(const std::vector<Variable>& variables,
                                                           Certificate& cert) const
{
    TestResult test{"Required Start Values", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        bool needs_start = false;

        // Rule 1: initial = "exact" or "approx" requires start
        if (var.initial == "exact" || var.initial == "approx")
            needs_start = true;

        // Rule 2: causality = "parameter" or "input" requires start
        if (var.causality == "parameter" || var.causality == "input")
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

void Fmi2ModelDescriptionChecker::checkCausalityVariabilityInitialCombinations(const std::vector<Variable>& variables,
                                                                               Certificate& cert) const
{
    TestResult test{"Causality/Variability/Initial Combinations", TestStatus::PASS, {}};

    // Legal combinations for FMI 2.0
    const std::set<std::tuple<std::string, std::string, std::string>> legal_combinations = {
        {"parameter", "fixed", "exact"},
        {"parameter", "tunable", "exact"},

        {"calculatedParameter", "fixed", "calculated"},
        {"calculatedParameter", "fixed", "approx"},
        {"calculatedParameter", "tunable", "calculated"},
        {"calculatedParameter", "tunable", "approx"},

        // FMI2: inputs must NOT have initial attribute (empty string)
        {"input", "discrete", ""},
        {"input", "continuous", ""},

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

        // FMI2: independent must NOT have initial attribute
        {"independent", "continuous", ""},
    };

    for (const auto& var : variables)
    {
        const std::string initial = var.initial.empty() ? "" : var.initial;
        auto combination = std::make_tuple(var.causality, var.variability, initial);

        if (!legal_combinations.contains(combination))
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Variable '{}' (line {}) has illegal combination: causality='{}", var.name, var.sourceline,
                            var.causality));
            ;
        }
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::checkIllegalStartValues(const std::vector<Variable>& variables,
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

        // FMI2: Independent variables should not have start values
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

void Fmi2ModelDescriptionChecker::checkMinMaxStartValues(const std::vector<Variable>& variables,
                                                         const std::map<std::string, TypeDefinition>& type_definitions,
                                                         Certificate& cert) const
{
    TestResult test{"Min/Max/Start Value Constraints", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        // Skip non-numeric types
        if (var.type != "Real" && var.type != "Integer" && var.type != "Enumeration")
            continue;

        // Get effective bounds (considering type definitions)
        const EffectiveBounds bounds = getEffectiveBounds(var, type_definitions);

        // Check nominal for special floats (not allowed in FMI 2.0)
        if (var.type == "Real" && var.nominal && isSpecialFloat(*var.nominal))
            validateVariableSpecialFloat(test, var, *var.nominal, "nominal");

        // Validate variable's bounds using the appropriate type
        if (var.type == "Real")
            validateTypeBounds<double>(var, bounds.min, bounds.max, test);
        else if (var.type == "Integer" || var.type == "Enumeration")
            validateTypeBounds<int32_t>(var, bounds.min, bounds.max, test);
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::checkModelStructure(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                      Certificate& cert) const
{
    validateOutputs(doc, variables, cert);
    validateDerivatives(doc, variables, cert);
    validateInitialUnknowns(doc, variables, cert);
}

void Fmi2ModelDescriptionChecker::validateOutputs(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                  Certificate& cert) const
{
    TestResult test{"ModelStructure Outputs", TestStatus::PASS, {}};

    auto get_base_type = [](const std::string& type) -> std::string
    {
        if (type == "Integer" || type == "Enumeration")
            return "Integer/Enumeration";
        return type;
    };

    using AliasKey = std::pair<std::string, uint32_t>;

    // Get expected outputs alias sets
    std::set<AliasKey> expected_alias_sets;
    for (const auto& var : variables)
        if (var.causality == "output" && var.value_reference.has_value())
            expected_alias_sets.insert({get_base_type(var.type), *var.value_reference});

    // FMI2: Get actual outputs from 'ModelStructure/Outputs'/Unknown (using index attribute)
    std::set<AliasKey> actual_alias_sets;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/Outputs/Unknown");

    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            const auto index_str = getXmlAttribute(node, "index");

            if (index_str.has_value())
            {
                if (const auto index_opt = parseNumber<size_t>(*index_str))
                {
                    const size_t index = *index_opt;
                    // FMI2 uses 1-based indexing
                    if (index > 0 && index <= variables.size())
                    {
                        const auto& var = variables[index - 1];
                        if (var.causality != "output")
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(std::format("Variable '{}' (line {}) listed in "
                                                                        "'ModelStructure/Outputs' but does not have "
                                                                        "causality='output'.",
                                                                        var.name, var.sourceline));
                        }

                        if (var.value_reference.has_value())
                        {
                            const AliasKey key = {get_base_type(var.type), *var.value_reference};
                            if (actual_alias_sets.contains(key))
                            {
                                test.setStatus(TestStatus::FAIL);
                                test.getMessages().emplace_back(std::format(
                                    "An alias of variable '{}' (VR {}, type {}) is already represented in "
                                    "'ModelStructure/Outputs'. Exactly one representative of an alias set must be "
                                    "listed.",
                                    var.name, *var.value_reference, var.type));
                            }
                            actual_alias_sets.insert(key);
                        }

                        // Check dependencies ordering and dependenciesKind consistency
                        const auto deps_str = getXmlAttribute(node, "dependencies");
                        const auto deps_kind_str = getXmlAttribute(node, "dependenciesKind");

                        if (deps_str)
                        {
                            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                            const auto& ds_val = *deps_str;
                            std::vector<size_t> deps;
                            std::stringstream ss(ds_val);
                            size_t dep_idx = 0;
                            while (ss >> dep_idx)
                                deps.push_back(dep_idx);

                            // Check ordering
                            for (size_t j = 1; j < deps.size(); ++j)
                            {
                                if (deps[j] <= deps[j - 1])
                                {
                                    test.setStatus(TestStatus::FAIL);
                                    test.getMessages().emplace_back(
                                        std::format("Variable '{}' (line {}) in 'ModelStructure/Outputs' has "
                                                    "dependencies that are not ordered according to magnitude.",
                                                    var.name, node->line));
                                    break;
                                }
                            }

                            // Check dependenciesKind size and values
                            if (deps_kind_str.has_value())
                            {
                                // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                                const auto& dk_val = *deps_kind_str;
                                std::vector<std::string> kinds;
                                std::stringstream ss_kind(dk_val);
                                std::string kind;
                                while (ss_kind >> kind)
                                    kinds.emplace_back(kind);

                                if (kinds.size() != deps.size())
                                {
                                    test.setStatus(TestStatus::FAIL);
                                    test.getMessages().emplace_back(
                                        std::format("Variable '{}' (line {}) in 'ModelStructure/Outputs' must have the "
                                                    "same number of list elements in "
                                                    "dependencies and dependenciesKind.",
                                                    var.name, node->line));
                                }

                                for (const auto& k : kinds)
                                {
                                    if (k == "constant" || k == "fixed" || k == "tunable" || k == "discrete")
                                    {
                                        if (var.type != "Real")
                                        {
                                            test.setStatus(TestStatus::FAIL);
                                            test.getMessages().emplace_back(std::format(
                                                "Variable '{}' (line {}) in 'ModelStructure/Outputs' has "
                                                "dependenciesKind='{}' which is only allowed for Real variables.",
                                                var.name, node->line, k));
                                        }
                                    }
                                }
                            }
                        }
                        else if (deps_kind_str.has_value())
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(std::format(
                                "Variable '{}' (line {}) in 'ModelStructure/Outputs': If 'dependenciesKind' is "
                                "present, 'dependencies' must be present.",
                                var.name, node->line));
                        }
                    }
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    if (expected_alias_sets != actual_alias_sets)
    {
        test.setStatus(TestStatus::FAIL);

        for (const auto& [type, vr] : expected_alias_sets)
        {
            if (!actual_alias_sets.contains({type, vr}))
            {
                test.getMessages().emplace_back(
                    std::format("Output alias set (VR {}, type {}) is missing a representative in "
                                "'ModelStructure/Outputs'.",
                                vr, type));
            }
        }

        test.getMessages().emplace_back(
            "'ModelStructure/Outputs' must have exactly one representative for each alias set of "
            "variables with causality='output'.");
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::validateDerivatives(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                      Certificate& cert) const
{
    TestResult test{"ModelStructure Derivatives", TestStatus::PASS, {}};

    std::map<uint32_t, const Variable*> index_map;
    for (const auto& var : variables)
        index_map[var.index] = &var;

    // FMI2: Check Derivatives entries (using index attribute)
    std::set<uint32_t> actual_vrs;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/Derivatives/Unknown");

    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            const auto index_str = getXmlAttribute(node, "index");

            if (index_str.has_value())
            {
                if (const auto index_opt = parseNumber<size_t>(*index_str))
                {
                    const size_t index = *index_opt;
                    if (index > 0 && index <= variables.size())
                    {
                        const auto& var = variables[index - 1];

                        // 1. Must be a Real variable with a derivative attribute
                        if (var.type != "Real" || !var.derivative_of.has_value())
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(std::format(
                                "Variable '{}' (line {}) listed in 'ModelStructure/Derivatives' must be of type 'Real' "
                                "and have a 'derivative' attribute.",
                                var.name, var.sourceline));
                        }
                        else
                        {
                            // 2. The variable that the derivative attribute points to (its state) should be Real with
                            // variability="continuous"
                            const uint32_t state_index = *var.derivative_of;
                            auto it = index_map.find(state_index);
                            if (it == index_map.end())
                            {
                                test.setStatus(TestStatus::FAIL);
                                test.getMessages().emplace_back(std::format(
                                    "Variable '{}' (line {}) has a 'derivative' attribute pointing to non-existent "
                                    "index {}.",
                                    var.name, var.sourceline, state_index));
                            }
                            else
                            {
                                const Variable* state_var = it->second;
                                if (state_var->type != "Real" || state_var->variability != "continuous")
                                {
                                    test.setStatus(TestStatus::FAIL);
                                    test.getMessages().emplace_back(std::format(
                                        "Variable '{}' (line {}) is listed in 'ModelStructure/Derivatives' and is a "
                                        "derivative of '{}' (line {}), which must be Real and have "
                                        "variability='continuous'.",
                                        var.name, var.sourceline, state_var->name, state_var->sourceline));
                                }
                            }
                        }

                        if (var.value_reference.has_value())
                        {
                            if (actual_vrs.contains(*var.value_reference))
                            {
                                test.setStatus(TestStatus::FAIL);
                                test.getMessages().emplace_back(std::format(
                                    "Value reference {} is listed multiple times in 'ModelStructure/Derivatives'.",
                                    *var.value_reference));
                            }
                            actual_vrs.insert(*var.value_reference);
                        }

                        // Check dependencies ordering and dependenciesKind consistency
                        const auto deps_str = getXmlAttribute(node, "dependencies");
                        const auto deps_kind_str = getXmlAttribute(node, "dependenciesKind");

                        if (deps_str)
                        {
                            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                            const auto& ds_val = *deps_str;
                            std::vector<size_t> deps;
                            std::stringstream ss(ds_val);
                            size_t dep_idx = 0;
                            while (ss >> dep_idx)
                                deps.push_back(dep_idx);

                            // Check ordering
                            for (size_t j = 1; j < deps.size(); ++j)
                            {
                                if (deps[j] <= deps[j - 1])
                                {
                                    test.setStatus(TestStatus::FAIL);
                                    test.getMessages().emplace_back(
                                        std::format("Variable '{}' (line {}) in 'ModelStructure/Derivatives' has "
                                                    "dependencies that are not ordered according to magnitude.",
                                                    var.name, node->line));
                                    break;
                                }
                            }

                            // Check dependenciesKind size and values
                            if (deps_kind_str.has_value())
                            {
                                // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                                const auto& dk_val = *deps_kind_str;
                                std::vector<std::string> kinds;
                                std::stringstream ss_kind(dk_val);
                                std::string kind;
                                while (ss_kind >> kind)
                                    kinds.emplace_back(kind);

                                if (kinds.size() != deps.size())
                                {
                                    test.setStatus(TestStatus::FAIL);
                                    test.getMessages().emplace_back(std::format(
                                        "Variable '{}' (line {}) in 'ModelStructure/Derivatives' must have the "
                                        "same number of list elements in dependencies and dependenciesKind.",
                                        var.name, node->line));
                                }

                                for (const auto& k : kinds)
                                {
                                    if (k == "constant" || k == "fixed" || k == "tunable" || k == "discrete")
                                    {
                                        if (var.type != "Real")
                                        {
                                            test.setStatus(TestStatus::FAIL);
                                            test.getMessages().emplace_back(std::format(
                                                "Variable '{}' (line {}) in 'ModelStructure/Derivatives' has "
                                                "dependenciesKind='{}' which is only allowed for Real variables.",
                                                var.name, node->line, k));
                                        }
                                    }
                                }
                            }
                        }
                        else if (deps_kind_str.has_value())
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(std::format(
                                "Variable '{}' (line {}) in 'ModelStructure/Derivatives': If 'dependenciesKind' is "
                                "present, 'dependencies' must be present.",
                                var.name, node->line));
                        }
                    }
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::validateInitialUnknowns(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                          Certificate& cert) const
{
    TestResult test{"ModelStructure Initial Unknowns", TestStatus::PASS, {}};

    const auto get_base_type = [](const std::string& type) -> std::string
    {
        if (type == "Integer" || type == "Enumeration")
            return "Integer/Enumeration";
        return type;
    };

    using AliasKey = std::pair<std::string, uint32_t>;

    // Build expected set of initial unknown alias sets (FMI2 spec)
    std::set<AliasKey> expected_alias_sets;

    // Identify active continuous-time states and derivatives
    std::set<uint32_t> active_state_indices;
    std::set<uint32_t> active_derivative_indices;

    xmlXPathObjectPtr xpath_der = getXPathNodes(doc, "//ModelStructure/Derivatives/Unknown");
    if (xpath_der != nullptr && xpath_der->nodesetval != nullptr)
    {
        for (int32_t i = 0; i < xpath_der->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = xpath_der->nodesetval->nodeTab[i];
            const auto index_str = getXmlAttribute(node, "index");
            if (index_str.has_value())
            {
                if (const auto index_opt = parseNumber<size_t>(*index_str))
                {
                    const size_t index = *index_opt;
                    if (index > 0 && index <= variables.size())
                    {
                        const auto& var = variables[index - 1];
                        active_derivative_indices.insert(var.index);
                        if (var.derivative_of.has_value())
                            active_state_indices.insert(*var.derivative_of);
                    }
                }
            }
        }
        xmlXPathFreeObject(xpath_der);
    }

    for (const auto& var : variables)
    {
        if (!var.value_reference.has_value())
            continue;

        bool is_mandatory = false;

        // (1) Outputs with initial="approx" or "calculated"
        if (var.causality == "output" && (var.initial == "approx" || var.initial == "calculated"))
            is_mandatory = true;

        // (2) Calculated parameters
        if (var.causality == "calculatedParameter")
            is_mandatory = true;

        // (3) Active states and their derivatives with initial="approx" or "calculated"
        if ((active_state_indices.contains(var.index) || active_derivative_indices.contains(var.index)) &&
            (var.initial == "approx" || var.initial == "calculated"))
        {
            is_mandatory = true;
        }

        if (is_mandatory)
            expected_alias_sets.insert({get_base_type(var.type), *var.value_reference});
    }

    // FMI2: Get actual initial unknowns (using index attribute)
    std::set<AliasKey> actual_alias_sets;
    std::vector<size_t> actual_indices;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/InitialUnknowns/Unknown");

    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            const auto index_str = getXmlAttribute(node, "index");

            if (index_str.has_value())
            {
                if (const auto index_opt = parseNumber<size_t>(*index_str))
                {
                    const size_t index = *index_opt;
                    if (index > 0 && index <= variables.size())
                    {
                        const auto& var = variables[index - 1];

                        if (var.value_reference.has_value())
                        {
                            const AliasKey key = {get_base_type(var.type), *var.value_reference};

                            if (actual_alias_sets.contains(key))
                            {
                                test.setStatus(TestStatus::FAIL);
                                test.getMessages().emplace_back(std::format(
                                    "An alias of variable '{}' (VR {}, type {}) is already represented in "
                                    "'ModelStructure/InitialUnknowns'. Exactly one representative of an alias set must "
                                    "be listed.",
                                    var.name, *var.value_reference, var.type));
                            }
                            actual_alias_sets.insert(key);

                            if (!expected_alias_sets.contains(key))
                            {
                                test.setStatus(TestStatus::FAIL);
                                test.getMessages().emplace_back(std::format(
                                    "Variable '{}' (line {}) is listed in 'ModelStructure/InitialUnknowns' but is not "
                                    "a mandatory unknown.",
                                    var.name, var.sourceline));
                            }
                        }

                        actual_indices.push_back(index);

                        // Check dependencies ordering and dependenciesKind consistency
                        const auto deps_str = getXmlAttribute(node, "dependencies");
                        const auto deps_kind_str = getXmlAttribute(node, "dependenciesKind");

                        if (deps_str)
                        {
                            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                            const auto& ds_val = *deps_str;
                            std::vector<size_t> deps;
                            std::stringstream ss(ds_val);
                            size_t dep_idx = 0;
                            while (ss >> dep_idx)
                                deps.push_back(dep_idx);

                            // Check ordering
                            for (size_t j = 1; j < deps.size(); ++j)
                            {
                                if (deps[j] <= deps[j - 1])
                                {
                                    test.setStatus(TestStatus::FAIL);
                                    test.getMessages().emplace_back(
                                        std::format("Variable '{}' (line {}) in 'ModelStructure/InitialUnknowns' has "
                                                    "dependencies that are not ordered "
                                                    "according to magnitude.",
                                                    var.name, node->line));
                                    break;
                                }
                            }

                            // Check dependenciesKind size and values
                            if (deps_kind_str.has_value())
                            {
                                // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                                const auto& dk_val = *deps_kind_str;
                                std::vector<std::string> kinds;
                                std::stringstream ss_kind(dk_val);
                                std::string kind;
                                while (ss_kind >> kind)
                                    kinds.push_back(kind);

                                if (kinds.size() != deps.size())
                                {
                                    test.setStatus(TestStatus::FAIL);
                                    test.getMessages().emplace_back(
                                        std::format("Variable '{}' (line {}) in 'ModelStructure/InitialUnknowns' must "
                                                    "have the same number of list "
                                                    "elements in dependencies and dependenciesKind.",
                                                    var.name, node->line));
                                }

                                for (const auto& k : kinds)
                                {
                                    if (k == "fixed" || k == "tunable" || k == "discrete")
                                    {
                                        test.setStatus(TestStatus::FAIL);
                                        test.getMessages().emplace_back(
                                            std::format("Variable '{}' (line {}) in 'ModelStructure/InitialUnknowns' "
                                                        "has dependenciesKind='{}' "
                                                        "which is not allowed in InitialUnknowns.",
                                                        var.name, node->line, k));
                                    }
                                    else if (k == "constant")
                                    {
                                        if (var.type != "Real")
                                        {
                                            test.setStatus(TestStatus::FAIL);
                                            test.getMessages().emplace_back(std::format(
                                                "Variable '{}' (line {}) in 'ModelStructure/InitialUnknowns' has "
                                                "dependenciesKind='{}' "
                                                "which is only allowed for Real variables.",
                                                var.name, node->line, k));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    // Check ordering of InitialUnknowns
    for (size_t i = 1; i < actual_indices.size(); ++i)
    {
        if (actual_indices[i] <= actual_indices[i - 1])
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back("'ModelStructure/InitialUnknowns' must be ordered according to their "
                                            "ScalarVariable index.");
            break;
        }
    }

    if (expected_alias_sets != actual_alias_sets)
    {
        test.setStatus(TestStatus::FAIL);

        for (const auto& [type, vr] : expected_alias_sets)
        {
            if (!actual_alias_sets.contains({type, vr}))
            {
                test.getMessages().emplace_back(
                    std::format("Mandatory initial unknown alias set (VR {}, type {}) is missing a representative in "
                                "'ModelStructure/InitialUnknowns'.",
                                vr, type));
            }
        }

        test.getMessages().emplace_back(
            "'ModelStructure/InitialUnknowns' must have exactly one representative for each mandatory alias set.");
    }

    cert.printTestResult(test);
}

std::map<std::string, TypeDefinition> Fmi2ModelDescriptionChecker::extractTypeDefinitions(xmlDocPtr doc) const
{
    std::map<std::string, TypeDefinition> type_definitions;

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//TypeDefinitions/SimpleType");
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
        xmlNodePtr simple_type_node = nodes->nodeTab[i];
        TypeDefinition type_def;

        // Get name attribute
        type_def.name = getXmlAttribute(simple_type_node, "name").value_or("");
        type_def.sourceline = simple_type_node->line;

        // Find the type element (Real, Integer, Boolean, String, Enumeration)
        for (xmlNodePtr child = simple_type_node->children; child != nullptr; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            const std::string elem_name = reinterpret_cast<const char*>(child->name);

            if (elem_name == "Real" || elem_name == "Integer" || elem_name == "Boolean" || elem_name == "String" ||
                elem_name == "Enumeration")
            {
                type_def.type = elem_name;

                // Extract min, max, nominal, unit, displayUnit attributes
                if (elem_name == "Real")
                {
                    auto rel_q = getXmlAttribute(child, "relativeQuantity");
                    if (rel_q)
                        type_def.relative_quantity = (*rel_q == "true");
                }
                type_def.min = getXmlAttribute(child, "min");
                type_def.max = getXmlAttribute(child, "max");
                type_def.nominal = getXmlAttribute(child, "nominal");
                type_def.unit = getXmlAttribute(child, "unit");
                type_def.display_unit = getXmlAttribute(child, "displayUnit");

                break;
            }
        }

        if (!type_def.name.empty())
            type_definitions[type_def.name] = type_def;
    }

    xmlXPathFreeObject(xpath_obj);
    return type_definitions;
}

void Fmi2ModelDescriptionChecker::validateVariableSpecialFloat(TestResult& test, const Variable& var,
                                                               const std::string& val,
                                                               const std::string& attr_name) const
{
    test.setStatus(TestStatus::FAIL);
    test.getMessages().emplace_back(std::format("Variable '{}' (line {}): {} value '{}' is {}, which is not allowed.",
                                                var.name, var.sourceline, attr_name, val,
                                                getSpecialFloatDescription(val)));
}

void Fmi2ModelDescriptionChecker::validateDefaultExperimentSpecialFloat(TestResult& test, const std::string& val,
                                                                        const std::string& attr_name) const
{
    test.setStatus(TestStatus::FAIL);
    test.getMessages().emplace_back(
        std::format("{} value '{}' is {}, which is not allowed.", attr_name, val, getSpecialFloatDescription(val)));
}

void Fmi2ModelDescriptionChecker::validateUnitSpecialFloat(TestResult& test, const std::string& val,
                                                           const std::string& attr_name, const std::string& context,
                                                           size_t line) const
{
    test.setStatus(TestStatus::FAIL);
    test.getMessages().emplace_back(std::format("{} (line {}): {} value '{}' is {}, which is not allowed.", context,
                                                line, attr_name, val, getSpecialFloatDescription(val)));
}

void Fmi2ModelDescriptionChecker::validateTypeDefinitionSpecialFloat(TestResult& test, const TypeDefinition& type_def,
                                                                     const std::string& val,
                                                                     const std::string& attr_name) const
{
    test.setStatus(TestStatus::FAIL);
    test.getMessages().emplace_back(
        std::format("Type definition '{}' (line {}): {} value '{}' is {}, which is not allowed.", type_def.name,
                    type_def.sourceline, attr_name, val, getSpecialFloatDescription(val)));
}

void Fmi2ModelDescriptionChecker::checkTypeDefinitions(xmlDocPtr doc, Certificate& cert) const
{
    TestResult test{"Type Definitions", TestStatus::PASS, {}};

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//TypeDefinitions/SimpleType");
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

        // Check types
        for (xmlNodePtr child = type_node->children; child != nullptr; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            const std::string elem_name = reinterpret_cast<const char*>(child->name);

            auto min_str = getXmlAttribute(child, "min");
            auto max_str = getXmlAttribute(child, "max");
            auto nominal_str = getXmlAttribute(child, "nominal");

            // Check for special floats (NaN, INF)
            TypeDefinition td_for_special;
            td_for_special.name = name;
            td_for_special.sourceline = child->line;

            if (min_str && isSpecialFloat(*min_str))
                validateTypeDefinitionSpecialFloat(test, td_for_special, *min_str, "min");
            if (max_str && isSpecialFloat(*max_str))
                validateTypeDefinitionSpecialFloat(test, td_for_special, *max_str, "max");
            if (nominal_str && isSpecialFloat(*nominal_str))
                validateTypeDefinitionSpecialFloat(test, td_for_special, *nominal_str, "nominal");

            if (min_str && max_str)
            {
                const bool special_min = isSpecialFloat(*min_str);
                const bool special_max = isSpecialFloat(*max_str);

                if (!special_min && !special_max)
                {
                    if (elem_name == "Real")
                    {
                        const auto min_val = parseNumber<double>(*min_str);
                        const auto max_val = parseNumber<double>(*max_str);
                        if (min_val && max_val)
                        {
                            if (*max_val < *min_val)
                            {
                                test.setStatus(TestStatus::FAIL);
                                test.getMessages().emplace_back(
                                    std::format("Type definition '{}' (line {}): max ({}) must be >= min ({}).", name,
                                                child->line, *max_str, *min_str));
                            }
                        }
                        else
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(std::format(
                                "Type definition '{}' (line {}): Failed to parse min/max values.", name, child->line));
                        }
                    }
                    else if (elem_name == "Integer" || elem_name == "Enumeration")
                    {
                        const auto min_val = parseNumber<int64_t>(*min_str);
                        const auto max_val = parseNumber<int64_t>(*max_str);
                        if (min_val && max_val)
                        {
                            if (*max_val < *min_val)
                            {
                                test.setStatus(TestStatus::FAIL);
                                test.getMessages().emplace_back(
                                    std::format("Type definition '{}' (line {}): max ({}) must be >= min ({}).", name,
                                                child->line, *max_str, *min_str));
                            }
                        }
                        else
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(std::format(
                                "Type definition '{}' (line {}): Failed to parse min/max values.", name, child->line));
                        }
                    }
                }
            }

            if (elem_name == "Enumeration")
            {
                bool has_items = false;
                std::set<int32_t> item_values;
                std::set<std::string> item_names;

                for (xmlNodePtr item = child->children; item != nullptr; item = item->next)
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
                                                child->line, *item_name));
                            }
                            item_names.insert(*item_name);
                        }

                        if (item_value_str)
                        {
                            if (const auto val = parseNumber<int32_t>(*item_value_str))
                            {
                                if (item_values.contains(*val))
                                {
                                    test.setStatus(TestStatus::FAIL);
                                    test.getMessages().emplace_back(
                                        "Enumeration type '" + name + "' (line " + std::to_string(child->line) +
                                        ") has multiple items with value " + *item_value_str +
                                        ". Item values must be unique within the same enumeration.");
                                }
                                item_values.insert(*val);
                            }
                        }
                    }
                }

                if (!has_items)
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format(
                        "Enumeration type '{}' (line {}): must have at least one Item.", name, child->line));
                }
            }
        }
    }

    xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::checkAnnotations(xmlDocPtr doc, Certificate& cert) const
{
    TestResult test{"Vendor Annotations Uniqueness", TestStatus::PASS, {}};

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//VendorAnnotations/Tool");
    if (xpath_obj == nullptr || xpath_obj->nodesetval == nullptr)
    {
        if (xpath_obj != nullptr)
            xmlXPathFreeObject(xpath_obj);
        cert.printTestResult(test);
        return;
    }

    std::set<std::string> seen_names;
    for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
        auto name = getXmlAttribute(node, "name");
        if (name)
        {
            if (seen_names.contains(*name))
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "Vendor annotation tool '{}' (line {}): is defined multiple times.", *name, node->line));
            }
            seen_names.insert(*name);
        }
    }

    xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::checkGenerationDateReleaseYear(const std::string& dt, std::time_t generation_time,
                                                                 TestResult& test) const
{
    checkGenerationDateReleaseYearBase(dt, generation_time, 2014, "2.0", test);
}

void Fmi2ModelDescriptionChecker::checkGuid(const std::optional<std::string>& guid_opt, Certificate& cert) const
{
    TestResult test{"GUID", TestStatus::PASS, {}};

    if (!guid_opt.has_value())
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back("guid attribute is missing.");
        cert.printTestResult(test);
        return;
    }

    if (guid_opt->empty())
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back("guid attribute is empty.");
        cert.printTestResult(test);
        return;
    }

    if (test.getStatus() != TestStatus::PASS)
        test.getMessages().emplace_back(std::format("GUID: {}", *guid_opt));

    const std::string& guid = *guid_opt;
    const std::regex guid_pattern(
        R"(^(\{)?[0-9a-fA-F]{8}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{12}(\})?$)");

    if (!std::regex_match(guid, guid_pattern))
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back(
            "guid '" + guid + "' does not match expected GUID format ({xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx})");
    }

    cert.printTestResult(test);
}

ModelMetadata Fmi2ModelDescriptionChecker::extractMetadata(xmlNodePtr root) const
{
    ModelMetadata metadata;
    metadata.fmi_version = getXmlAttribute(root, "fmiVersion");
    metadata.model_name = getXmlAttribute(root, "modelName");
    metadata.guid = getXmlAttribute(root, "guid");
    metadata.model_version = getXmlAttribute(root, "version");
    metadata.author = getXmlAttribute(root, "author");
    metadata.copyright = getXmlAttribute(root, "copyright");
    metadata.license = getXmlAttribute(root, "license");
    metadata.description = getXmlAttribute(root, "description");
    metadata.generation_tool = getXmlAttribute(root, "generationTool");
    metadata.generation_date_and_time = getXmlAttribute(root, "generationDateAndTime");
    metadata.variable_naming_convention = getXmlAttribute(root, "variableNamingConvention").value_or("flat");

    auto num_event_ind = getXmlAttribute(root, "numberOfEventIndicators");
    if (num_event_ind)
        metadata.number_of_event_indicators = parseNumber<uint32_t>(*num_event_ind);

    return metadata;
}

void Fmi2ModelDescriptionChecker::checkUnits(xmlDocPtr doc, Certificate& cert) const
{
    TestResult test{"Unit Definitions", TestStatus::PASS, {}};

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//UnitDefinitions/Unit");
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
        std::set<std::string> unit_display_names;

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

        for (xmlNodePtr child = unit_node->children; child != nullptr; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            const std::string elem_name = reinterpret_cast<const char*>(child->name);

            if (elem_name == "BaseUnit")
            {
                checkSpecial(getXmlAttribute(child, "factor"), "factor", "Unit '" + name + "' BaseUnit", child->line);
                checkSpecial(getXmlAttribute(child, "offset"), "offset", "Unit '" + name + "' BaseUnit", child->line);
            }
            else if (elem_name == "DisplayUnit")
            {
                auto du_name_opt = getXmlAttribute(child, "name");
                const std::string du_name = du_name_opt.value_or("unnamed");

                if (du_name_opt)
                {
                    if (unit_display_names.contains(*du_name_opt))
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(
                            std::format("DisplayUnit '{}' (line {}) is defined multiple times for unit '{}'.",
                                        *du_name_opt, child->line, name));
                    }
                    unit_display_names.insert(*du_name_opt);
                }

                const std::string context = std::format("Unit '{}' DisplayUnit '{}'", name, du_name);
                checkSpecial(getXmlAttribute(child, "factor"), "factor", context, child->line);
                checkSpecial(getXmlAttribute(child, "offset"), "offset", context, child->line);
            }
        }
    }

    xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

std::map<std::string, UnitDefinition> Fmi2ModelDescriptionChecker::extractUnitDefinitions(xmlDocPtr doc) const
{
    std::map<std::string, UnitDefinition> units;

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//UnitDefinitions/Unit");
    if (xpath_obj == nullptr)
        return units;

    xmlNodeSetPtr nodes = xpath_obj->nodesetval;
    if (nodes == nullptr)
    {
        xmlXPathFreeObject(xpath_obj);
        return units;
    }

    for (int32_t i = 0; i < nodes->nodeNr; ++i)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr unit_node = nodes->nodeTab[i];
        UnitDefinition unit_def;

        unit_def.name = getXmlAttribute(unit_node, "name").value_or("");
        unit_def.sourceline = unit_node->line;

        if (unit_def.name.empty())
            continue;

        for (xmlNodePtr child = unit_node->children; child != nullptr; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            const std::string elem_name = reinterpret_cast<const char*>(child->name);

            if (elem_name == "BaseUnit")
            {
                auto base_factor = getXmlAttribute(child, "factor");
                auto base_offset = getXmlAttribute(child, "offset");
                if (base_factor)
                    unit_def.factor = base_factor;
                if (base_offset)
                    unit_def.offset = base_offset;
            }
            else if (elem_name == "DisplayUnit")
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

void Fmi2ModelDescriptionChecker::checkSourceFilesSemantic(xmlDocPtr doc, Certificate& cert) const
{
    TestResult test{"Source Files Semantic Validation", TestStatus::PASS, {}};

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//SourceFiles/File");
    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        for (int i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            auto name_opt = getXmlAttribute(node, "name");
            if (name_opt)
            {
                auto file_path = getFmuRootPath() / "sources" / (*name_opt);
                if (!std::filesystem::exists(file_path))
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format("Source file '{}' listed in 'modelDescription.xml' "
                                                                "(line {}) does not exist in 'sources/' directory.",
                                                                *name_opt, node->line));
                }
            }
        }
    }
    if (xpath_obj != nullptr)
        xmlXPathFreeObject(xpath_obj);

    cert.printTestResult(test);
}
