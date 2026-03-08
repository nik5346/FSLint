#include "fmi1_model_description_checker.h"

#include "model_description_checker.h"

#include "certificate.h"

#include <libxml/tree.h>
#include <libxml/xpath.h>

// NOLINTBEGIN(misc-include-cleaner)
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
// NOLINTEND(misc-include-cleaner)

#include "format_shim.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <utility>
#include <vector>

void Fmi1ModelDescriptionChecker::performVersionSpecificChecks(
    xmlDocPtr doc, const std::vector<Variable>& variables,
    [[maybe_unused]] const std::map<std::string, TypeDefinition>& type_definitions,
    [[maybe_unused]] const std::map<std::string, UnitDefinition>& units, Certificate& cert)
{
    // Check Alias variables
    checkAliases(variables, cert);

    // Check implementation (CoSimulation only)
    checkImplementation(doc, cert);
}

void Fmi1ModelDescriptionChecker::validateFmiVersionValue(const std::string& version, TestResult& test)
{
    if (version != "1.0")
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("version \"" + version + "\" is invalid (must be exactly \"1.0\").");
    }
}

void Fmi1ModelDescriptionChecker::checkGuid(const std::optional<std::string>& guid, Certificate& cert)
{
    TestResult test{"GUID Format", TestStatus::PASS, {}};
    if (!guid.has_value())
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("guid attribute is missing.");
        cert.printTestResult(test);
        return;
    }

    if (guid->empty())
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("guid attribute is empty.");
        cert.printTestResult(test);
        return;
    }

    static const std::regex guid_pattern(
        R"(^(\{)?[0-9a-fA-F]{8}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{12}(\})?$)");

    if (!std::regex_match(*guid, guid_pattern))
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("guid \"" + *guid +
                                "\" does not match expected GUID format ({xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx})");
    }

    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::checkGenerationDateReleaseYear(const std::string& dt, std::time_t generation_time,
                                                                 TestResult& test)
{
    checkGenerationDateReleaseYearBase(dt, generation_time, 2010, "1.0", test);
}

void Fmi1ModelDescriptionChecker::checkAnnotations(xmlDocPtr doc, Certificate& cert)
{
    TestResult test{"Vendor Annotations Uniqueness", TestStatus::PASS, {}};
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/VendorAnnotations/Tool");
    if (xpath_obj && xpath_obj->nodesetval)
    {
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
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Vendor annotation tool \"" + *name + "\" (line " +
                                            std::to_string(node->line) + ") is defined multiple times.");
                }
                seen_names.insert(*name);
            }
        }
    }
    if (xpath_obj)
        xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::applyDefaultInitialValues(std::vector<Variable>& variables)
{
    for (auto& var : variables)
    {
        if (!var.initial.empty())
            continue;

        // FMI 1.0 default for 'fixed' (which we mapped to initial)
        // Table in 3.3 says for Real: "fixed: ... = true: ... this is the default."
        // But it's only allowed if causality is NOT input.
        if (var.causality != "input")
            var.initial = "exact";
    }
}

void Fmi1ModelDescriptionChecker::checkCausalityVariabilityInitialCombinations(const std::vector<Variable>& variables,
                                                                               Certificate& cert)
{
    TestResult test{"Causality/Variability/Initial Combinations", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        if (var.variability == "constant")
        {
            if (var.causality == "input")
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") has illegal combination: variability=\"constant\" and causality=\"input\". "
                                        "Logical contradiction: constants cannot be changed from the outside.");
            }
        }
    }
    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Legal Variability", TestStatus::PASS, {}};
    for (const auto& var : variables)
    {
        // Only Real can be continuous
        if (var.variability == "continuous")
        {
            if (var.type != "Real")
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") is of type " + var.type + " and cannot have variability \"continuous\".");
            }
        }
    }
    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::checkRequiredStartValues(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Required Start Values", TestStatus::PASS, {}};
    for (const auto& var : variables)
    {
        bool needs_start = false;
        // FMI 1.0 ME Spec 3.3: "A variable of causality = “input”, must have a “start” value."
        // FMI 1.0 ME Spec 3.3 Note: "all constants, independent parameters and inputs of the FMU must have a start
        // value" Since we cannot distinguish "independent" parameters from calculated ones without the start value, we
        // only enforce it for input and constant.
        if (var.causality == "input" || var.variability == "constant")
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

void Fmi1ModelDescriptionChecker::checkIllegalStartValues(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Illegal Start Values", TestStatus::PASS, {}};
    for (const auto& var : variables)
    {
        // FMI 1.0: "fixed" attribute is only allowed if "start" is present
        if (var.fixed.has_value() && !var.start.has_value())
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") has 'fixed' attribute but is missing 'start' value.");
        }

        // FMI 1.0: "fixed" attribute is not defined for causality="input"
        if (var.causality == "input" && var.fixed.has_value())
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") has causality=\"input\" and a 'fixed' attribute. The 'fixed' attribute is only "
                                    "defined for causalities other than 'input' (Section 3.3).");
        }

        // FMI 1.0: "fixed" attribute for variability="constant"
        if (var.variability == "constant" && var.fixed.has_value() && !var.fixed.value())
        {
            // fixed="false" (guess value) makes no sense for a constant
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") has variability=\"constant\" and fixed=\"false\", which is a contradiction.");
        }
    }
    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::checkMinMaxStartValues(const std::vector<Variable>& variables,
                                                         const std::map<std::string, TypeDefinition>& type_definitions,
                                                         Certificate& cert)
{
    TestResult test{"Min/Max/Start Value Constraints", TestStatus::PASS, {}};
    for (const auto& var : variables)
    {
        if (var.type != "Real" && var.type != "Integer" && var.type != "Enumeration")
            continue;

        const EffectiveBounds bounds = getEffectiveBounds(var, type_definitions);
        if (var.type == "Real")
            validateTypeBounds<double>(var, bounds.min, bounds.max, test);
        else if (var.type == "Integer" || var.type == "Enumeration")
            validateTypeBounds<int32_t>(var, bounds.min, bounds.max, test);
    }
    cert.printTestResult(test);
}

std::map<std::string, std::string> Fmi1ModelDescriptionChecker::extractModelIdentifiers(
    xmlDocPtr doc, [[maybe_unused]] const std::vector<std::string>& interface_elements)
{
    std::map<std::string, std::string> model_identifiers;
    xmlNodePtr root = xmlDocGetRootElement(doc);
    auto model_id = getXmlAttribute(root, "modelIdentifier");
    if (model_id)
    {
        bool is_cs = false;
        xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/Implementation");
        if (xpath_obj && xpath_obj->nodesetval && xpath_obj->nodesetval->nodeNr > 0)
            is_cs = true;
        if (xpath_obj)
            xmlXPathFreeObject(xpath_obj);

        if (is_cs)
            model_identifiers["CoSimulation"] = *model_id;
        else
            model_identifiers["ModelExchange"] = *model_id;
    }
    return model_identifiers;
}

ModelMetadata Fmi1ModelDescriptionChecker::extractMetadata(xmlNodePtr root)
{
    ModelMetadata metadata;
    metadata.fmiVersion = getXmlAttribute(root, "fmiVersion");
    metadata.modelName = getXmlAttribute(root, "modelName");
    metadata.guid = getXmlAttribute(root, "guid");
    metadata.modelVersion = getXmlAttribute(root, "version");
    metadata.author = getXmlAttribute(root, "author");
    metadata.generationTool = getXmlAttribute(root, "generationTool");
    metadata.generationDateAndTime = getXmlAttribute(root, "generationDateAndTime");
    metadata.variableNamingConvention = getXmlAttribute(root, "variableNamingConvention").value_or("flat");

    auto num_states = getXmlAttribute(root, "numberOfContinuousStates");
    // We don't have a place for numberOfContinuousStates in metadata currently, but it's used in directory check?
    // Actually ModelMetadata doesn't have it.

    auto num_event_ind = getXmlAttribute(root, "numberOfEventIndicators");
    if (num_event_ind)
        metadata.numberOfEventIndicators = parseNumber<uint32_t>(*num_event_ind);
    return metadata;
}

std::map<std::string, UnitDefinition> Fmi1ModelDescriptionChecker::extractUnitDefinitions(xmlDocPtr doc)
{
    std::map<std::string, UnitDefinition> units;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/UnitDefinitions/BaseUnit");
    if (!xpath_obj || !xpath_obj->nodesetval)
    {
        if (xpath_obj)
            xmlXPathFreeObject(xpath_obj);
        return units;
    }

    for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr unit_node = xpath_obj->nodesetval->nodeTab[i];
        UnitDefinition unit_def;
        unit_def.name = getXmlAttribute(unit_node, "unit").value_or("");
        unit_def.sourceline = unit_node->line;
        if (unit_def.name.empty())
            continue;

        for (xmlNodePtr child = unit_node->children; child; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            const std::string elem_name = reinterpret_cast<const char*>(child->name);
            if (elem_name == "DisplayUnitDefinition")
            {
                auto du_name = getXmlAttribute(child, "displayUnit");
                if (du_name)
                {
                    DisplayUnit du;
                    du.name = *du_name;
                    du.factor = getXmlAttribute(child, "gain"); // FMI1 uses gain
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

std::map<std::string, TypeDefinition> Fmi1ModelDescriptionChecker::extractTypeDefinitions(xmlDocPtr doc)
{
    std::map<std::string, TypeDefinition> type_definitions;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/TypeDefinitions/Type");
    if (!xpath_obj || !xpath_obj->nodesetval)
    {
        if (xpath_obj)
            xmlXPathFreeObject(xpath_obj);
        return type_definitions;
    }

    for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr type_node = xpath_obj->nodesetval->nodeTab[i];
        TypeDefinition type_def;
        type_def.name = getXmlAttribute(type_node, "name").value_or("");
        type_def.sourceline = type_node->line;

        for (xmlNodePtr child = type_node->children; child; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            const std::string elem_name = reinterpret_cast<const char*>(child->name);
            if (elem_name == "RealType" || elem_name == "IntegerType" || elem_name == "BooleanType" ||
                elem_name == "StringType" || elem_name == "EnumerationType")
            {
                type_def.type = elem_name.substr(0, elem_name.length() - 4);
                type_def.min = getXmlAttribute(child, "min");
                type_def.max = getXmlAttribute(child, "max");
                type_def.nominal = getXmlAttribute(child, "nominal");
                type_def.unit = getXmlAttribute(child, "unit");
                type_def.display_unit = getXmlAttribute(child, "displayUnit");
                auto rel_q = getXmlAttribute(child, "relativeQuantity");
                if (rel_q)
                    type_def.relative_quantity = (*rel_q == "true");
                break;
            }
        }
        if (!type_def.name.empty())
            type_definitions[type_def.name] = type_def;
    }
    xmlXPathFreeObject(xpath_obj);
    return type_definitions;
}

std::vector<Variable> Fmi1ModelDescriptionChecker::extractVariables(xmlDocPtr doc)
{
    _is_cs = false;
    xmlXPathObjectPtr xpath_obj_impl = getXPathNodes(doc, "/fmiModelDescription/Implementation");
    if (xpath_obj_impl && xpath_obj_impl->nodesetval && xpath_obj_impl->nodesetval->nodeNr > 0)
        _is_cs = true;
    if (xpath_obj_impl)
        xmlXPathFreeObject(xpath_obj_impl);

    std::vector<Variable> variables;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/ModelVariables/ScalarVariable");
    if (!xpath_obj || !xpath_obj->nodesetval)
    {
        if (xpath_obj)
            xmlXPathFreeObject(xpath_obj);
        return variables;
    }

    for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr scalar_var_node = xpath_obj->nodesetval->nodeTab[i];
        Variable var;
        var.name = getXmlAttribute(scalar_var_node, "name").value_or("");
        var.causality = getXmlAttribute(scalar_var_node, "causality").value_or("internal");
        var.variability = getXmlAttribute(scalar_var_node, "variability").value_or("");
        var.declared_type = getXmlAttribute(scalar_var_node, "declaredType");
        var.sourceline = scalar_var_node->line;
        var.index = static_cast<uint32_t>(i + 1);
        var.alias = getXmlAttribute(scalar_var_node, "alias");

        auto vr = getXmlAttribute(scalar_var_node, "valueReference");
        if (vr)
            var.value_reference = parseNumber<uint32_t>(*vr);

        for (xmlNodePtr child = scalar_var_node->children; child; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            const std::string elem_name = reinterpret_cast<const char*>(child->name);
            if (elem_name == "Real" || elem_name == "Integer" || elem_name == "Boolean" || elem_name == "String" ||
                elem_name == "Enumeration")
            {
                var.type = elem_name;
                var.start = getXmlAttribute(child, "start");
                var.min = getXmlAttribute(child, "min");
                var.max = getXmlAttribute(child, "max");
                var.nominal = getXmlAttribute(child, "nominal");
                var.unit = getXmlAttribute(child, "unit");
                var.display_unit = getXmlAttribute(child, "displayUnit");

                auto fixed = getXmlAttribute(child, "fixed");
                if (fixed)
                {
                    var.fixed = (*fixed == "true");
                    var.initial = (var.fixed.value() ? "exact" : "approx");
                }

                if (elem_name == "Real")
                {
                    auto rel_q = getXmlAttribute(child, "relativeQuantity");
                    if (rel_q)
                        var.relative_quantity = (*rel_q == "true");
                }
                break;
            }
        }

        if (var.variability.empty())
        {
            if (var.type == "Real")
                var.variability = "continuous";
            else
                var.variability = "discrete";
        }

        variables.push_back(var);
    }
    xmlXPathFreeObject(xpath_obj);
    return variables;
}

void Fmi1ModelDescriptionChecker::checkUnits(xmlDocPtr doc, Certificate& cert)
{
    TestResult test{"Unit Definitions", TestStatus::PASS, {}};
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/UnitDefinitions/BaseUnit");
    if (xpath_obj && xpath_obj->nodesetval)
    {
        std::set<std::string> seen_names;
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            auto name = getXmlAttribute(node, "unit");
            if (name)
            {
                if (seen_names.contains(*name))
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Unit \"" + *name + "\" (line " + std::to_string(node->line) +
                                            ") is defined multiple times.");
                }
                seen_names.insert(*name);
            }
        }
    }
    if (xpath_obj)
        xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::checkTypeDefinitions(xmlDocPtr doc, Certificate& cert)
{
    TestResult test{"Type Definitions", TestStatus::PASS, {}};
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/TypeDefinitions/Type");
    if (xpath_obj && xpath_obj->nodesetval)
    {
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
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Type definition \"" + *name + "\" (line " + std::to_string(node->line) +
                                            ") is defined multiple times.");
                }
                seen_names.insert(*name);
            }
        }
    }
    if (xpath_obj)
        xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::validateVariableSpecialFloat(TestResult& /*test*/, const Variable& /*var*/,
                                                               const std::string& /*val*/,
                                                               const std::string& /*attr_name*/)
{
    // Special floats are allowed in FMI 1.0
}

void Fmi1ModelDescriptionChecker::validateDefaultExperimentSpecialFloat(TestResult& /*test*/,
                                                                        const std::string& /*val*/,
                                                                        const std::string& /*attr_name*/)
{
    // Special floats are allowed in FMI 1.0
}

void Fmi1ModelDescriptionChecker::validateUnitSpecialFloat(TestResult& /*test*/, const std::string& /*val*/,
                                                           const std::string& /*attr_name*/,
                                                           const std::string& /*unit_name*/, size_t /*line*/)
{
    // Special floats are allowed in FMI 1.0
}

void Fmi1ModelDescriptionChecker::validateTypeDefinitionSpecialFloat(TestResult& /*test*/,
                                                                     const TypeDefinition& /*type_def*/,
                                                                     const std::string& /*val*/,
                                                                     const std::string& /*attr_name*/)
{
    // Special floats are allowed in FMI 1.0
}

void Fmi1ModelDescriptionChecker::checkModelIdentifier(const std::string& model_identifier,
                                                       const std::string& interface_name, Certificate& cert)
{
    ModelDescriptionCheckerBase::checkModelIdentifier(model_identifier, interface_name, cert);
    checkModelIdentifierMatch(model_identifier, cert);
}

void Fmi1ModelDescriptionChecker::checkModelIdentifierMatch(const std::string& model_identifier, Certificate& cert)
{
    const auto& original_path = getOriginalPath();
    const auto& fmu_root_path = getFmuRootPath();

    // If original_path is not set, we can't perform this check reliably in some contexts,
    // but in normal execution it should be set. If it's empty, we fall back to root path stem.
    const auto& path_to_check = original_path.empty() ? fmu_root_path : original_path;
    const std::string expected_id = path_to_check.stem().string();

    if (model_identifier != expected_id)
    {
        cert.printTestResult({"Model Identifier Filename Match",
                              TestStatus::FAIL,
                              {fslint::format("modelIdentifier '{}' must match the FMU filename '{}'.",
                                              model_identifier, expected_id)}});
    }
    else
    {
        cert.printTestResult({"Model Identifier Filename Match", TestStatus::PASS, {}});
    }
}

void Fmi1ModelDescriptionChecker::checkImplementation(xmlDocPtr doc, Certificate& cert)
{
    // For FMI 1.0, the presence of the Implementation element distinguishes Co-Simulation from Model Exchange.
    // If present, we validate its contents (CoSimulation_StandAlone or CoSimulation_Tool).
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/Implementation");
    if (xpath_obj && xpath_obj->nodesetval && xpath_obj->nodesetval->nodeNr > 0)
    {
        TestResult test{"CS Implementation", TestStatus::PASS, {}};
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr impl_node = xpath_obj->nodesetval->nodeTab[0];
        xmlNodePtr tool_node = nullptr;

        for (xmlNodePtr child = impl_node->children; child; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            const std::string name = reinterpret_cast<const char*>(child->name);
            if (name == "CoSimulation_StandAlone" || name == "CoSimulation_Tool")
            {
                if (name == "CoSimulation_Tool")
                    tool_node = child;
                break;
            }
        }

        if (tool_node)
        {
            // For CoSimulation_Tool, check Model element and its attributes
            xmlNodePtr model_node = nullptr;
            for (xmlNodePtr child = tool_node->children; child; child = child->next)
            {
                if (child->type != XML_ELEMENT_NODE)
                    continue;
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                if (reinterpret_cast<const char*>(child->name) == std::string("Model"))
                {
                    model_node = child;
                    break;
                }
            }

            if (model_node)
            {
                auto entry_point = getXmlAttribute(model_node, "entryPoint");
                if (entry_point)
                    checkUri(*entry_point, "entryPoint", model_node->line, test);

                // Check additional files
                for (xmlNodePtr child = model_node->children; child; child = child->next)
                {
                    if (child->type != XML_ELEMENT_NODE)
                        continue;
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                    if (reinterpret_cast<const char*>(child->name) == std::string("File"))
                    {
                        auto file_uri = getXmlAttribute(child, "file");
                        if (file_uri)
                            checkUri(*file_uri, "file", child->line, test);
                    }
                }
            }
        }
        else
        {
            // If Implementation is present but no CoSimulation_StandAlone/Tool, it's malformed according to schema
            // but we check for it anyway if it's not handled by schema checker.
            // Actually, if tool_node is null and it's not StandAlone, it's an error.
            bool is_standalone = false;
            for (xmlNodePtr child = impl_node->children; child; child = child->next)
            {
                if (child->type != XML_ELEMENT_NODE)
                    continue;
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                if (reinterpret_cast<const char*>(child->name) == std::string("CoSimulation_StandAlone"))
                {
                    is_standalone = true;
                    break;
                }
            }
            if (!is_standalone)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back(
                    "Implementation element must contain either CoSimulation_StandAlone or CoSimulation_Tool.");
            }
        }

        cert.printTestResult(test);
    }
    if (xpath_obj)
        xmlXPathFreeObject(xpath_obj);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Fmi1ModelDescriptionChecker::checkUri(const std::string& uri, const std::string& attr_name, int line,
                                           TestResult& test)
{
    if (uri.compare(0, 6, "fmu://") == 0)
    {
        std::string relative_path = uri.substr(6); // Remove "fmu://"
        // The path in fmu:// scheme is relative to the FMU root.
        // We remove any leading slash if present after fmu:// (e.g., fmu:///resources/...)
        if (!relative_path.empty() && relative_path[0] == '/')
            relative_path.erase(0, 1);

        const std::filesystem::path full_path = getFmuRootPath() / relative_path;
        if (!std::filesystem::exists(full_path))
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Attribute '" + attr_name + "' (line " + std::to_string(line) +
                                    ") references missing file in FMU: '" + relative_path + "'.");
        }
    }
    else if (uri.compare(0, 7, "file://") == 0)
    {
        std::string path_str = uri.substr(7);
        if (path_str.empty())
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Attribute '" + attr_name + "' (line " + std::to_string(line) +
                                    ") has an empty file:// URI.");
        }
        else
        {
            // Simple heuristic for absolute path: starts with / or [A-Z]:
            const bool is_absolute = (path_str[0] == '/') || (path_str.size() > 1 && path_str[1] == ':');
            if (is_absolute)
            {
                // On Windows, if it starts with /C:/, remove the leading / for std::filesystem
                if (path_str.size() > 2 && path_str[0] == '/' && path_str[2] == ':')
                    path_str.erase(0, 1);

                if (!std::filesystem::exists(path_str))
                {
                    if (test.status == TestStatus::PASS)
                        test.status = TestStatus::WARNING;
                    test.messages.push_back("Attribute '" + attr_name + "' (line " + std::to_string(line) +
                                            ") references an external file that does not exist on this system: '" +
                                            uri + "'. This may affect portability.");
                }
            }
        }
    }
    else if (uri.compare(0, 7, "http://") == 0 || uri.compare(0, 8, "https://") == 0)
    {
        // Restrictive regex for URL validation to prevent command injection and ensure standard compliance.
        static const std::regex url_regex(R"(^https?://[a-zA-Z0-9\-\._~:/?#%@\+&!=\[\]]+$)", std::regex::optimize);
        if (!std::regex_match(uri, url_regex))
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Attribute '" + attr_name + "' (line " + std::to_string(line) +
                                    ") has an invalid or unsafe HTTP/HTTPS URI: '" + uri + "'.");
        }
        else if (!checkReachability(uri))
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("Attribute '" + attr_name + "' (line " + std::to_string(line) +
                                    ") references a web source that appears to be unreachable: '" + uri + "'.");
        }
    }
    else
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Attribute '" + attr_name + "' (line " + std::to_string(line) +
                                ") has an unsupported or invalid URI scheme: '" + uri + "'.");
    }
}

// NOLINTBEGIN(misc-include-cleaner)
#ifdef _WIN32
static int runProcess(const std::string& url)
{
    // Build the command line — no shell, direct curl invocation
    std::string cmdLine = "curl -I -s -L --max-time 5 --fail \"" + url + "\"";

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    // Suppress curl output by redirecting handles to NUL
    HANDLE hNull = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hNull;
    si.hStdError = hNull;

    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr,
                        /*bInheritHandles=*/TRUE, 0, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(hNull);
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hNull);
    return static_cast<int>(exitCode);
}
#else
static int runProcess(const std::string& url)
{
    pid_t pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0) // child
    {
        // Redirect stdout/stderr to /dev/null
        int fd = open("/dev/null", O_WRONLY);
        if (fd >= 0)
        {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        // execvp does NOT invoke a shell
        execlp("curl", "curl", "-I", "-s", "-L", "--max-time", "5", "--fail", url.c_str(), nullptr);
        _exit(127); // exec failed
    }

    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
#endif
// NOLINTEND(misc-include-cleaner)

bool Fmi1ModelDescriptionChecker::checkReachability(const std::string& url)
{
    static const std::regex safe_url_regex(R"(^https?://[a-zA-Z0-9\-\._~:/?#%@\+&!=\[\]]+$)", std::regex::optimize);
    if (!std::regex_match(url, safe_url_regex))
        return false;

    return runProcess(url) == 0;
}

void Fmi1ModelDescriptionChecker::checkAliases(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Alias Variables", TestStatus::PASS, {}};

    auto get_base_type = [](const std::string& type) -> std::string
    {
        if (type == "Integer" || type == "Enumeration")
            return "Integer/Enumeration";
        return type;
    };

    // Group variables by base type and valueReference
    // In FMI 1.0, valueReference is unique only per base type.
    std::map<std::pair<std::string, uint32_t>, std::vector<const Variable*>> alias_sets;
    for (const auto& var : variables)
        if (var.value_reference.has_value())
            alias_sets[{get_base_type(var.type), *var.value_reference}].push_back(&var);

    for (const auto& [key, alias_set] : alias_sets)
    {
        if (alias_set.size() <= 1)
            continue;

        const auto& [base_type, vr] = key;
        const Variable* first = alias_set[0];
        const Variable* first_with_start = nullptr;
        double first_normalized_start = 0.0;

        for (const auto* var : alias_set)
        {
            // 1. Same base type
            if (var->type != first->type)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back(
                    fslint::format("Variables sharing VR {} must have the same type. \"{}\" is {} but \"{}\" is {}.",
                                   vr, var->name, var->type, first->name, first->type));
            }

            // 2. If Real, same unit
            if (base_type == "Real" && var->unit != first->unit)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back(fslint::format(
                    "Variables sharing VR {} must have the same unit. \"{}\" has unit "
                    "\"{}\" but \"{}\" has unit \"{}\".",
                    vr, var->name, var->unit.value_or("(none)"), first->name, first->unit.value_or("(none)")));
            }

            // 3. Equivalent start values
            if (var->start.has_value())
            {
                double current_val = 0;
                bool valid = false;
                const bool negated = (var->alias && *var->alias == "negatedAlias");

                if (base_type == "Real")
                {
                    if (const auto val = parseNumber<double>(*var->start))
                    {
                        current_val = (negated ? -*val : *val);
                        valid = true;
                    }
                }
                else if (base_type == "Integer/Enumeration")
                {
                    if (const auto val = parseNumber<int32_t>(*var->start))
                    {
                        current_val = static_cast<double>(negated ? -*val : *val);
                        valid = true;
                    }
                }
                else if (base_type == "Boolean")
                {
                    if (const auto val = parseNumber<int32_t>(*var->start))
                    {
                        // In FMI 1.0 Booleans are 0 or 1.
                        current_val = static_cast<double>(negated ? -*val : *val);
                        valid = true;
                    }
                }

                if (valid)
                {
                    if (!first_with_start)
                    {
                        first_with_start = var;
                        first_normalized_start = current_val;
                    }
                    else
                    {
                        // Use a small epsilon for float comparison, exact for others
                        const double eps = (base_type == "Real") ? 1e-10 : 0.0;
                        if (std::abs(current_val - first_normalized_start) > eps)
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back(fslint::format(
                                "Variables sharing VR {} must have equivalent start values. \"{}\" has start=\"{}\" "
                                "(normalized: {}) but \"{}\" has start=\"{}\" (normalized: {}).",
                                vr, var->name, *var->start, current_val, first_with_start->name,
                                *first_with_start->start, first_normalized_start));
                        }
                    }
                }
            }
        }
    }

    cert.printTestResult(test);
}
