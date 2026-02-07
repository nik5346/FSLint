#include "fmi3_model_description_checker.h"
#include "certificate.h"
#include <regex>

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include <iostream>
#include <tuple>

void Fmi3ModelDescriptionChecker::performVersionSpecificChecks(
    xmlDocPtr doc, const std::vector<Variable>& variables,
    [[maybe_unused]] const std::map<std::string, TypeDefinition>& type_definitions,
    [[maybe_unused]] const std::map<std::string, UnitDefinition>& units, Certificate& cert)
{
    // FMI3-specific checks
    checkIndependentVariable(variables, cert);
    checkDimensionReferences(variables, cert);
    checkArrayStartValues(variables, cert);
    checkClockReferences(variables, cert);
    checkClockedVariables(variables, cert);
    checkAliases(variables, cert);
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
        var.min = getXmlAttribute(node, "min");
        var.max = getXmlAttribute(node, "max");
        var.nominal = getXmlAttribute(node, "nominal");

        // FMI3: Check for Start element child for String/Binary types
        if (!var.start.has_value() && (var.type == "String" || var.type == "Binary"))
        {
            // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
            for (xmlNodePtr child = node->children; child; child = child->next)
            {
                if (child->type == XML_ELEMENT_NODE &&
                    xmlStrcmp(child->name, reinterpret_cast<const xmlChar*>("Start")) == 0)
                {
                    var.start = getXmlAttribute(child, "value");
                    break;
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

        auto vr = getXmlAttribute(node, "valueReference");
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

        // FMI3: derivative attribute is on the variable element itself
        auto der = getXmlAttribute(node, "derivative");
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
        if (var.causality == "structuralParameter")
        {
            if (var.variability == "fixed" || var.variability == "tunable")
                var.initial = "exact";
        }
        else if (var.causality == "parameter")
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
    TestResult test{"Legal Variability (FMI3)", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        // FMI3: Non-Real types (Float32, Float64) cannot be continuous
        if (var.type != "Float32" && var.type != "Float64" && var.variability == "continuous")
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back(
                "Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) + ") is of type " + var.type +
                " and cannot have variability \"continuous\". Only variables of type Float32 or Float64 can be continuous.");
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkRequiredStartValues(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Required Start Values (FMI3)", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        // Skip Clock types (FMI3)
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
    TestResult test{"Causality/Variability/Initial Combinations (FMI3)", TestStatus::PASS, {}};

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
        std::string initial = var.initial.empty() ? "" : var.initial;
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
    TestResult test{"Illegal Start Values (FMI3)", TestStatus::PASS, {}};

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
                                                    ") must be >= min (" + *type_def.min + ").");
                        }
                    }
                    catch (...)
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("Type definition \"" + type_def.name + "\" (line " +
                                                std::to_string(type_def.sourceline) +
                                                "): Failed to parse min/max values.");
                    }
                }
            }
        }

        // Validate variable's bounds using the appropriate type
        if (var.type == "Float32")
            validateTypeBounds<float>(var, bounds.min, bounds.max, test);
        else if (var.type == "Float64")
            validateTypeBounds<double>(var, bounds.min, bounds.max, test);
        else if (var.type == "Enumeration")
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
        else if (var.type == "Int64")
            validateTypeBounds<int64_t>(var, bounds.min, bounds.max, test);
        else if (var.type == "UInt64")
            validateTypeBounds<uint64_t>(var, bounds.min, bounds.max, test);
    }

    cert.printTestResult(test);
}
void Fmi3ModelDescriptionChecker::checkIndependentVariable(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Independent Variable (FMI3)", TestStatus::PASS, {}};

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
                                        std::to_string(var.sourceline) +
                                        ") must not have an initial attribute (FMI3 spec).");
            }

            // FMI3: Check for illegal start attribute
            if (var.start.has_value())
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Independent variable \"" + var.name + "\" (line " +
                                        std::to_string(var.sourceline) +
                                        ") must not have a start attribute (FMI3 spec).");
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

void Fmi3ModelDescriptionChecker::checkAliases(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Alias Variables (FMI3)", TestStatus::PASS, {}};

    // Group variables by valueReference
    std::map<uint32_t, std::vector<const Variable*>> vr_to_vars;
    for (const auto& var : variables)
    {
        if (var.value_reference.has_value())
            vr_to_vars[*var.value_reference].push_back(&var);
    }

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
                                        var->name + "\" has unit \"" + var->unit.value_or("(none)") +
                                        "\" but \"" + first->name + "\" has unit \"" +
                                        first->unit.value_or("(none)") + "\".");
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
                test.messages.push_back("Variables sharing VR " + std::to_string(vr) +
                                        " must have the same variability. \"" + var->name + "\" is " +
                                        var->variability + " but \"" + first->name + "\" is " + first->variability +
                                        ".");
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
        {
            if (v->causality != "local")
                non_local.push_back(v);
        }

        if (non_local.size() > 1)
        {
            test.status = TestStatus::FAIL;
            std::string msg = "Alias set for VR " + std::to_string(vr) +
                              " has multiple variables with causality other than 'local': ";
            for (size_t i = 0; i < non_local.size(); ++i)
                msg += (i > 0 ? ", " : "") + non_local[i]->name;
            msg += ". At most one variable in an alias set can be non-local (parameter, input, output, or independent).";
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
                    test.messages.push_back("Aliased constant variables \"" + var->name + "\" and \"" +
                                            first->name + "\" (sharing VR " + std::to_string(vr) +
                                            ") have different start values ('" + var->start.value_or("") +
                                            "' vs '" + first->start.value_or("") + "').");
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
            msg += ". At most one variable in an alias set (where at least one is not constant) can have a start attribute.";
            test.messages.push_back(msg);
        }
    }

    cert.printTestResult(test);
}

void Fmi3ModelDescriptionChecker::checkStructuralParameter(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Structural Parameter Validation (FMI3)", TestStatus::PASS, {}};

    // Build map of structural parameters
    std::map<std::string, const Variable*> structural_params;
    for (const auto& var : variables)
    {
        if (var.causality == "structuralParameter")
        {
            structural_params[var.name] = &var;

            // FMI3: Structural parameters must be UInt64
            if (var.type != "UInt64")
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Structural parameter \"" + var.name + "\" (line " +
                                        std::to_string(var.sourceline) + ") must be of type UInt64, found " + var.type + ".");
            }
        }
    }

    // Check variables with dimensions
    for (const auto& var : variables)
    {
        if (var.has_dimension && !var.dimension_refs.empty())
        {
            for (const auto& dim_ref : var.dimension_refs)
            {
                // Check if the dimension reference points to a structural parameter
                if (!structural_params.contains(dim_ref))
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                            ") references dimension \"" + dim_ref +
                                            "\" which is not a structural parameter.");
                }
                else
                {
                    // Check that the structural parameter has start > 0
                    const auto* sp = structural_params[dim_ref];
                    if (sp->start.has_value())
                    {
                        try
                        {
                            uint64_t start_val = std::stoull(*sp->start);
                            if (start_val == 0)
                            {
                                test.status = TestStatus::FAIL;
                                test.messages.push_back("Structural parameter \"" + sp->name + "\" (line " +
                                                        std::to_string(sp->sourceline) +
                                                        ") is referenced in <Dimension> and must have start > 0.");
                            }
                        }
                        catch (...)
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Structural parameter \"" + sp->name + "\" (line " +
                                                    std::to_string(sp->sourceline) +
                                                    ") has invalid start value (not a valid UInt64).");
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
    validateInitialUnknowns(doc, variables, cert);
}

void Fmi3ModelDescriptionChecker::validateOutputs(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                  Certificate& cert)
{
    TestResult test{"ModelStructure Outputs (FMI3)", TestStatus::PASS, {}};

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
                uint32_t vr = 0;
                try
                {
                    vr = std::stoul(*vr_str);
                }
                catch (...)
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("ModelStructure/Output " + std::to_string(i + 1) +
                                            " has invalid valueReference \"" + *vr_str + "\".");
                    continue;
                }

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

        for (uint32_t vr : expected_vrs)
            if (!actual_vrs.contains(vr))
                missing.push_back(vr_to_name[vr] + " (VR " + std::to_string(vr) + ")");

        for (uint32_t vr : actual_vrs)
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

void Fmi3ModelDescriptionChecker::validateDerivatives(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                      Certificate& cert)
{
    TestResult test{"ModelStructure Derivatives (FMI3)", TestStatus::PASS, {}};

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
                uint32_t vr = 0;
                try
                {
                    vr = std::stoul(*vr_str);
                }
                catch (...)
                {
                    continue;
                }

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

        for (uint32_t vr : expected_vrs)
            if (!actual_vrs.contains(vr))
                missing.push_back(vr_to_name[vr] + " (VR " + std::to_string(vr) + ")");

        for (uint32_t vr : actual_vrs)
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

void Fmi3ModelDescriptionChecker::validateInitialUnknowns(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                          Certificate& cert)
{
    TestResult test{"ModelStructure Initial Unknowns (FMI3)", TestStatus::PASS, {}};

    // Build expected set of initial unknown value references (FMI3 spec)
    std::set<uint32_t> expected_vrs;
    std::map<uint32_t, std::string> vr_to_name;

    for (const auto& var : variables)
    {
        if (!var.value_reference.has_value())
            continue;

        bool is_required = false;

        // (1) Outputs with initial="approx" or "calculated" (not clocked)
        if (var.causality == "output" && (var.initial == "approx" || var.initial == "calculated") &&
            !var.clocks.has_value())
        {
            is_required = true;
        }
        // (2) Calculated parameters
        else if (var.causality == "calculatedParameter")
        {
            is_required = true;
        }
        // (3) State derivatives with initial="approx" or "calculated"
        else if (var.derivative_of.has_value() && (var.initial == "approx" || var.initial == "calculated"))
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
                try
                {
                    uint32_t vr = std::stoul(*vr_str);
                    actual_vrs.insert(vr);
                }
                catch (...)
                {
                }
            }
        }
        xmlXPathFreeObject(xpath_obj);
    }

    if (expected_vrs != actual_vrs)
    {
        test.status = TestStatus::FAIL;

        std::string expected_str;
        for (uint32_t vr : expected_vrs)
            expected_str += vr_to_name[vr] + " (VR " + std::to_string(vr) + "), ";
        if (!expected_str.empty())
            expected_str = expected_str.substr(0, expected_str.length() - 2);

        std::string actual_str;
        for (uint32_t vr : actual_vrs)
            actual_str += "VR " + std::to_string(vr) + ", ";
        if (!actual_str.empty())
            actual_str = actual_str.substr(0, actual_str.length() - 2);

        test.messages.push_back("ModelStructure/InitialUnknown does not contain the expected set of variables. "
                                "Expected { " +
                                expected_str + " } but was { " + actual_str + " }.");
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
        std::string elem_name =
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
            xmlStrcmp(child->name, reinterpret_cast<const xmlChar*>("Dimension")) == 0)
        {
            var.has_dimension = true;

            Dimension dim;
            dim.sourceline = child->line;

            // Extract start attribute (fixed dimension size)
            auto start_attr = getXmlAttribute(child, "start");
            if (start_attr.has_value())
            {
                try
                {
                    dim.start = std::stoull(*start_attr);
                }
                catch (...)
                {
                    // Invalid start value will be caught in validation
                }
            }

            // Extract valueReference attribute (reference to structural parameter)
            auto vr_attr = getXmlAttribute(child, "valueReference");
            if (vr_attr.has_value())
            {
                try
                {
                    dim.value_reference = std::stoul(*vr_attr);
                }
                catch (...)
                {
                    // Invalid value reference will be caught in validation
                }
            }

            var.dimensions.push_back(dim);
        }
    }
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
}

void Fmi3ModelDescriptionChecker::checkDimensionReferences(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Dimension References (FMI3)", TestStatus::PASS, {}};

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
                bool has_start = dim.start.has_value();
                bool has_vr = dim.value_reference.has_value();

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
                    uint32_t vr = *dim.value_reference;
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
                            try
                            {
                                uint64_t start_val = std::stoull(*sp->start);
                                if (start_val == 0)
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
                            catch (...)
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
    TestResult test{"Array Start Values (FMI3)", TestStatus::PASS, {}};

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
                            try
                            {
                                uint64_t dim_size = std::stoull(*sp->start);
                                total_size = *total_size * dim_size;
                                dimension_info.push_back(sp->name + "=" + std::to_string(dim_size));
                            }
                            catch (...)
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

                size_t num_start_values = 0;

                // For most types, start is a space or comma-separated string
                if (var.type != "String" && var.type != "Binary")
                {
                    std::string start_str = *var.start;

                    // Remove leading/trailing whitespace
                    size_t first = start_str.find_first_not_of(" \t\n\r");
                    size_t last = start_str.find_last_not_of(" \t\n\r");
                    if (first != std::string::npos && last != std::string::npos)
                        start_str = start_str.substr(first, last - first + 1);

                    if (!start_str.empty())
                    {
                        // Better approach: actually split and count non-empty tokens
                        num_start_values = 0;
                        std::stringstream ss(start_str);
                        std::string token;
                        while (ss >> token)
                        {
                            // Remove commas
                            token.erase(std::remove(token.begin(), token.end(), ','), token.end());
                            if (!token.empty())
                                num_start_values++;
                        }
                    }
                }

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
            try
            {
                uint32_t vr = std::stoul(vr_str);
                clock_refs.push_back(vr);
            }
            catch (...)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        "): Invalid clock reference '" + vr_str + "' in clocks attribute.");
                continue;
            }
        }

        // Validate each clock reference
        for (uint32_t clock_vr : clock_refs)
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
                                    "): Parameters (causality='" + var.causality + "') cannot have a clocks attribute.");
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

void Fmi3ModelDescriptionChecker::validateDefaultExperimentSpecialFloat(TestResult& test, const std::string& val,
                                                                        const std::string& attr_name)
{
    // In FMI 3.0, stopTime="INF" is explicitly allowed and common
    if (attr_name == "stopTime" && (val.find("INF") != std::string::npos || val.find("inf") != std::string::npos))
        return;

    if (test.status == TestStatus::PASS)
        test.status = TestStatus::WARNING;

    test.messages.push_back(attr_name + " value \"" + val +
                            "\" is NaN or Infinity. While allowed in FMI 3.0, it is unusual in DefaultExperiment.");
}

void Fmi3ModelDescriptionChecker::validateUnitSpecialFloat(TestResult& test, const std::string& val,
                                                           const std::string& attr_name, const std::string& context,
                                                           size_t line)
{
    if (test.status == TestStatus::PASS)
        test.status = TestStatus::WARNING;

    test.messages.push_back(context + " (line " + std::to_string(line) + "): " + attr_name + " value \"" + val +
                            "\" is NaN or Infinity. While allowed in FMI 3.0, it is unusual.");
}

void Fmi3ModelDescriptionChecker::validateTypeDefinitionSpecialFloat(TestResult& /*test*/,
                                                                     const TypeDefinition& /*type_def*/,
                                                                     const std::string& /*val*/,
                                                                     const std::string& /*attr_name*/)
{
    // Special floats are allowed in FMI 3.0 type definitions
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
    std::regex guid_pattern(
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
    metadata.generationTool = getXmlAttribute(root, "generationTool");
    metadata.generationDateAndTime = getXmlAttribute(root, "generationDateAndTime");
    metadata.variableNamingConvention = getXmlAttribute(root, "variableNamingConvention").value_or("flat");

    auto num_event_ind = getXmlAttribute(root, "numberOfEventIndicators");
    if (num_event_ind)
    {
        try
        {
            metadata.numberOfEventIndicators = std::stoul(*num_event_ind);
        }
        catch (...)
        {
        }
    }

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
        std::string name = name_opt.value_or("unnamed");

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

            std::string elem_name =
                reinterpret_cast<const char*>(child->name); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

            if (elem_name == "DisplayUnit")
            {
                auto du_name = getXmlAttribute(child, "name").value_or("unnamed");
                checkSpecial(getXmlAttribute(child, "factor"), "factor",
                             "Unit \"" + name + "\" DisplayUnit \"" + du_name + "\"", child->line);
                checkSpecial(getXmlAttribute(child, "offset"), "offset",
                             "Unit \"" + name + "\" DisplayUnit \"" + du_name + "\"", child->line);
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

            std::string elem_name =
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
