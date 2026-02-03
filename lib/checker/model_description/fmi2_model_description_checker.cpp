#include "fmi2_model_description_checker.h"
#include "certificate.h"

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include <iostream>
#include <tuple>

void Fmi2ModelDescriptionChecker::performVersionSpecificChecks(
    xmlDocPtr doc, const std::vector<Variable>& variables,
    [[maybe_unused]] const std::map<std::string, TypeDefinition>& type_definitions,
    [[maybe_unused]] const std::map<std::string, UnitDefinition>& units, Certificate& cert)
{
    // Extract and check model identifiers for FMI2
    auto model_identifiers = extractModelIdentifiers(doc, {"CoSimulation", "ModelExchange"});
    checkNumberOfImplementedInterfaces(model_identifiers, cert);
    for (const auto& [interface_name, model_id] : model_identifiers)
        checkModelIdentifier(model_id, interface_name, cert);

    // Run FMI2-specific model structure checks
    checkModelStructure(doc, variables, cert);
}

std::vector<Variable> Fmi2ModelDescriptionChecker::extractVariables(xmlDocPtr doc)
{
    std::vector<Variable> variables;

    // FMI2 uses ModelVariables/ScalarVariable structure
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelVariables/ScalarVariable");
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
        xmlNodePtr scalar_var_node = nodes->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        Variable var;

        // Get attributes from ScalarVariable element
        var.name = getXmlAttribute(scalar_var_node, "name").value_or("");
        var.causality = getXmlAttribute(scalar_var_node, "causality").value_or("local");
        var.variability = getXmlAttribute(scalar_var_node, "variability").value_or("");
        var.initial = getXmlAttribute(scalar_var_node, "initial").value_or("");
        var.sourceline = scalar_var_node->line;

        auto vr = getXmlAttribute(scalar_var_node, "valueReference");
        if (vr.has_value())
        {
            try
            {
                var.value_reference = std::stoul(*vr);
            }
            catch (...)
            {
            }
        }

        // FMI2: The type element (Real, Integer, Boolean, String, Enumeration) is a child of ScalarVariable
        for (xmlNodePtr child = scalar_var_node->children; child; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            std::string elem_name =
                reinterpret_cast<const char*>(child->name); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            if (elem_name == "Real" || elem_name == "Integer" || elem_name == "Boolean" || elem_name == "String" ||
                elem_name == "Enumeration")
            {
                var.type = elem_name;

                // Get type-specific attributes
                var.declared_type = getXmlAttribute(child, "declaredType");
                var.start = getXmlAttribute(child, "start");
                var.min = getXmlAttribute(child, "min");
                var.max = getXmlAttribute(child, "max");
                var.unit = getXmlAttribute(child, "unit");
                var.display_unit = getXmlAttribute(child, "displayUnit");

                // FMI2: derivative attribute is on the Real element
                if (elem_name == "Real")
                {
                    auto der = getXmlAttribute(child, "derivative");
                    if (der.has_value())
                    {
                        try
                        {
                            var.derivative_of = std::stoul(*der);
                        }
                        catch (...)
                        {
                        }
                    }
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

void Fmi2ModelDescriptionChecker::applyDefaultInitialValues(std::vector<Variable>& variables)
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

void Fmi2ModelDescriptionChecker::checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Legal Variability (FMI2)", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        // FMI2: Only Real types can be continuous
        if (var.type != "Real" && var.variability == "continuous")
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") is of type " + var.type + " and must have variability != \"continuous\"");
        }
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::checkRequiredStartValues(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Required Start Values (FMI2)", TestStatus::PASS, {}};

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
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") must have a start value");
        }
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::checkCausalityVariabilityInitialCombinations(const std::vector<Variable>& variables,
                                                                               Certificate& cert)
{
    TestResult test{"Causality/Variability/Initial Combinations (FMI2)", TestStatus::PASS, {}};

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
        std::string initial = var.initial.empty() ? "" : var.initial;
        auto combination = std::make_tuple(var.causality, var.variability, initial);

        if (!legal_combinations.contains(combination))
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") has illegal combination: causality=\"" + var.causality + "\", variability=\"" +
                                    var.variability + "\", initial=\"" + initial + "\"");
        }
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::checkIllegalStartValues(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Illegal Start Values (FMI2)", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        // Variables with initial="calculated" should not have start values
        if (var.initial == "calculated" && var.start.has_value())
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") has initial=\"calculated\" but provides a start value");
        }

        // FMI2: Independent variables should not have start values
        if (var.causality == "independent" && var.start.has_value())
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") has causality=\"independent\" but provides a start value");
        }
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::checkMinMaxStartValues(const std::vector<Variable>& variables,
                                                         const std::map<std::string, TypeDefinition>& type_definitions,
                                                         Certificate& cert)
{
    TestResult test{"Min/Max/Start Value Constraints", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        // Skip non-numeric types
        if (var.type != "Real" && var.type != "Integer" && var.type != "Enumeration")
            continue;

        // Get effective bounds (considering type definitions)
        EffectiveBounds bounds = getEffectiveBounds(var, type_definitions);

        // First validate type definition's own min/max consistency
        if (var.declared_type)
        {
            auto it = type_definitions.find(*var.declared_type);
            if (it != type_definitions.end())
            {
                const auto& type_def = it->second;
                if (type_def.min && type_def.max)
                {
                    try
                    {
                        double type_min = std::stod(*type_def.min);
                        double type_max = std::stod(*type_def.max);

                        if (type_max < type_min)
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Type definition \"" + type_def.name + "\" (line " +
                                                    std::to_string(type_def.sourceline) + "): max (" + *type_def.max +
                                                    ") must be >= min (" + *type_def.min + ")");
                        }
                    }
                    catch (...)
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("Type definition \"" + type_def.name + "\" (line " +
                                                std::to_string(type_def.sourceline) +
                                                "): Failed to parse min/max values");
                    }
                }
            }
        }

        // Validate variable's bounds using the appropriate type
        if (var.type == "Real")
            validateTypeBounds<double>(var, bounds.min, bounds.max, test);
        else if (var.type == "Integer" || var.type == "Enumeration")
            validateTypeBounds<int32_t>(var, bounds.min, bounds.max, test);
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::checkModelStructure(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                      Certificate& cert)
{
    validateOutputs(doc, variables, cert);
    validateDerivatives(doc, variables, cert);
    validateInitialUnknowns(doc, variables, cert);
}

void Fmi2ModelDescriptionChecker::validateOutputs(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                  Certificate& cert)
{
    TestResult test{"ModelStructure Outputs (FMI2)", TestStatus::PASS, {}};

    // Get expected outputs (all variables with causality="output")
    std::set<std::string> expected_outputs;
    for (const auto& var : variables)
        if (var.causality == "output")
            expected_outputs.insert(var.name);

    // FMI2: Get actual outputs from ModelStructure/Outputs/Unknown (using index attribute)
    std::set<std::string> actual_outputs;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/Outputs/Unknown");

    if (xpath_obj && xpath_obj->nodesetval)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            xmlNodePtr node =
                xpath_obj->nodesetval->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            auto index_str = getXmlAttribute(node, "index");

            if (index_str.has_value())
            {
                try
                {
                    size_t index = std::stoul(*index_str);
                    // FMI2 uses 1-based indexing
                    if (index > 0 && index <= variables.size())
                    {
                        const auto& var = variables[index - 1];
                        if (var.causality != "output")
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Variable \"" + var.name + "\" (line " +
                                                    std::to_string(var.sourceline) +
                                                    ") listed in ModelStructure/Outputs but does not have "
                                                    "causality=\"output\"");
                        }

                        if (actual_outputs.contains(var.name))
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Variable \"" + var.name + "\" is listed multiple times in "
                                                    "ModelStructure/Outputs");
                        }
                        actual_outputs.insert(var.name);
                    }
                }
                catch (...)
                {
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    if (expected_outputs != actual_outputs)
    {
        test.status = TestStatus::FAIL;
        std::vector<std::string> missing;
        std::vector<std::string> extra;

        for (const auto& name : expected_outputs)
            if (!actual_outputs.contains(name))
                missing.push_back(name);

        for (const auto& name : actual_outputs)
            if (!expected_outputs.contains(name))
                extra.push_back(name);

        if (!missing.empty())
        {
            std::string msg = "The following variables with causality=\"output\" are missing from ModelStructure/Outputs: ";
            for (size_t i = 0; i < missing.size(); ++i)
                msg += (i > 0 ? ", " : "") + missing[i];
            test.messages.push_back(msg);
        }

        if (!extra.empty())
        {
            std::string msg = "The following variables in ModelStructure/Outputs do not have causality=\"output\": ";
            for (size_t i = 0; i < extra.size(); ++i)
                msg += (i > 0 ? ", " : "") + extra[i];
            test.messages.push_back(msg);
        }

        test.messages.push_back(
            "ModelStructure/Outputs must have exactly one entry for each variable with causality=\"output\"");
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::validateDerivatives(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                      Certificate& cert)
{
    TestResult test{"ModelStructure Derivatives (FMI2)", TestStatus::PASS, {}};

    // Build map of variables that have derivatives
    std::set<std::string> expected_derivatives;
    for (const auto& var : variables)
        if (var.derivative_of.has_value())
            expected_derivatives.insert(var.name);

    // FMI2: Check Derivatives entries (using index attribute)
    std::set<std::string> actual_derivatives;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/Derivatives/Unknown");

    if (xpath_obj && xpath_obj->nodesetval)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            xmlNodePtr node =
                xpath_obj->nodesetval->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            auto index_str = getXmlAttribute(node, "index");

            if (index_str.has_value())
            {
                try
                {
                    size_t index = std::stoul(*index_str);
                    if (index > 0 && index <= variables.size())
                    {
                        const auto& var = variables[index - 1];

                        if (!expected_derivatives.contains(var.name))
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Variable \"" + var.name + "\" (line " +
                                                    std::to_string(var.sourceline) + ") referenced by derivative " +
                                                    std::to_string(i + 1) + " must have the \"derivative\" attribute");
                        }

                        if (actual_derivatives.contains(var.name))
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Variable \"" + var.name + "\" is listed multiple times in "
                                                    "ModelStructure/Derivatives");
                        }
                        actual_derivatives.insert(var.name);
                    }
                }
                catch (...)
                {
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    if (expected_derivatives != actual_derivatives)
    {
        test.status = TestStatus::FAIL;
        std::vector<std::string> missing;
        std::vector<std::string> extra;

        for (const auto& name : expected_derivatives)
            if (!actual_derivatives.contains(name))
                missing.push_back(name);

        for (const auto& name : actual_derivatives)
            if (!expected_derivatives.contains(name))
                extra.push_back(name);

        if (!missing.empty())
        {
            std::string msg = "The following variables with a \"derivative\" attribute are missing from ModelStructure/Derivatives: ";
            for (size_t i = 0; i < missing.size(); ++i)
                msg += (i > 0 ? ", " : "") + missing[i];
            test.messages.push_back(msg);
        }

        if (!extra.empty())
        {
            std::string msg = "The following variables in ModelStructure/Derivatives do not have a \"derivative\" attribute: ";
            for (size_t i = 0; i < extra.size(); ++i)
                msg += (i > 0 ? ", " : "") + extra[i];
            test.messages.push_back(msg);
        }

        test.messages.push_back(
            "ModelStructure/Derivatives must have exactly one entry for each variable that has a \"derivative\" "
            "attribute");
    }

    cert.printTestResult(test);
}

void Fmi2ModelDescriptionChecker::validateInitialUnknowns(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                          Certificate& cert)
{
    TestResult test{"ModelStructure Initial Unknowns (FMI2)", TestStatus::PASS, {}};

    // Build expected set of initial unknowns (FMI2 spec)
    std::set<std::string> expected;

    for (const auto& var : variables)
    {
        // Outputs with initial="approx" or "calculated"
        if (var.causality == "output" && (var.initial == "approx" || var.initial == "calculated"))
            expected.insert(var.name);

        // Calculated parameters
        if (var.causality == "calculatedParameter")
            expected.insert(var.name);

        // States and their derivatives with initial="approx" or "calculated"
        if (var.derivative_of.has_value() && (var.initial == "approx" || var.initial == "calculated"))
        {
            expected.insert(var.name);

            // Also add the state itself if it has initial="approx" or "calculated"
            if (*var.derivative_of > 0 && *var.derivative_of <= variables.size())
            {
                const auto& state = variables[*var.derivative_of - 1];
                if (state.initial == "approx" || state.initial == "calculated")
                    expected.insert(state.name);
            }
        }
    }

    // FMI2: Get actual initial unknowns (using index attribute)
    std::set<std::string> actual;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ModelStructure/InitialUnknowns/Unknown");

    if (xpath_obj && xpath_obj->nodesetval)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            xmlNodePtr node =
                xpath_obj->nodesetval->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            auto index_str = getXmlAttribute(node, "index");

            if (index_str.has_value())
            {
                try
                {
                    size_t index = std::stoul(*index_str);
                    if (index > 0 && index <= variables.size())
                        actual.insert(variables[index - 1].name);
                }
                catch (...)
                {
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    if (expected != actual)
    {
        test.status = TestStatus::WARNING;

        std::string expected_str;
        for (const auto& name : expected)
            expected_str += name + ", ";
        if (!expected_str.empty())
            expected_str = expected_str.substr(0, expected_str.length() - 2);

        std::string actual_str;
        for (const auto& name : actual)
            actual_str += name + ", ";
        if (!actual_str.empty())
            actual_str = actual_str.substr(0, actual_str.length() - 2);

        test.messages.push_back("ModelStructure/InitialUnknowns does not contain the expected set of variables. "
                                "Expected { " +
                                expected_str + " } but was { " + actual_str + " }");
    }

    cert.printTestResult(test);
}

std::map<std::string, TypeDefinition> Fmi2ModelDescriptionChecker::extractTypeDefinitions(xmlDocPtr doc)
{
    std::map<std::string, TypeDefinition> type_definitions;

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//TypeDefinitions/SimpleType");
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
        xmlNodePtr simple_type_node = nodes->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        TypeDefinition type_def;

        // Get name attribute
        type_def.name = getXmlAttribute(simple_type_node, "name").value_or("");
        type_def.sourceline = simple_type_node->line;

        // Find the type element (Real, Integer, Boolean, String, Enumeration)
        for (xmlNodePtr child = simple_type_node->children; child; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            std::string elem_name =
                reinterpret_cast<const char*>(child->name); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            if (elem_name == "Real" || elem_name == "Integer" || elem_name == "Boolean" || elem_name == "String" ||
                elem_name == "Enumeration")
            {
                type_def.type = elem_name;

                // Extract min, max, unit, displayUnit attributes
                type_def.min = getXmlAttribute(child, "min");
                type_def.max = getXmlAttribute(child, "max");
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
