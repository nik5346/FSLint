#include "fmi1_model_description_checker.h"

#include "model_description_checker.h"

#include "certificate.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

void Fmi1ModelDescriptionChecker::performVersionSpecificChecks(
    xmlDocPtr doc, const std::vector<Variable>& variables,
    [[maybe_unused]] const std::map<std::string, TypeDefinition>& type_definitions,
    [[maybe_unused]] const std::map<std::string, UnitDefinition>& units, Certificate& cert) const
{
    // Check Alias variables
    checkAliases(variables, cert);

    // Check implementation (CoSimulation only)
    checkImplementation(doc, cert);

    // Check DirectDependency references
    checkDirectDependencies(doc, variables, cert);
}

void Fmi1ModelDescriptionChecker::validateFmiVersionValue(const std::string& version, TestResult& test) const
{
    if (version != "1.0")
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back(std::format("version '{}' is invalid (must be exactly '1.0').", version));
    }
}

void Fmi1ModelDescriptionChecker::checkGuid(const std::optional<std::string>& guid, Certificate& cert) const
{
    TestResult test{"GUID", TestStatus::PASS, {}};
    if (!guid)
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back("guid attribute is missing.");
        cert.printTestResult(test);
        return;
    }

    if (guid->empty())
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back("guid attribute is empty.");
        cert.printTestResult(test);
        return;
    }

    if (test.getStatus() != TestStatus::PASS)
        test.getMessages().emplace_back(std::format("GUID: {}", *guid));

    static const std::regex guid_pattern(
        R"(^(\{)?[0-9a-fA-F]{8}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{12}(\})?$)");

    if (!std::regex_match(*guid, guid_pattern))
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back(std::format(
            "guid '{{}}' does not match expected GUID format ({{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}})", *guid));
    }

    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::checkDirectDependencies(xmlDocPtr doc, const std::vector<Variable>& variables,
                                                          Certificate& cert) const
{
    TestResult test{"Direct Dependency References", TestStatus::PASS, {}};

    // Create a map for fast lookup of variables by name
    std::unordered_map<std::string, const Variable*> variable_map;
    for (const auto& var : variables)
        variable_map[var.name] = &var;

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//ScalarVariable[DirectDependency]");
    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            xmlNodePtr var_node = xpath_obj->nodesetval->nodeTab[i];
            auto var_name = getXmlAttribute(var_node, "name");
            auto causality = getXmlAttribute(var_node, "causality").value_or("internal");

            if (causality != "output")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "Variable '{}' (line {}) has a DirectDependency element but causality is '{}' (must be 'output').",
                    var_name.value_or("unknown"), var_node->line, causality));
            }

            // Find DirectDependency node
            xmlNodePtr dd_node = nullptr;
            for (xmlNodePtr child = var_node->children; child != nullptr; child = child->next)
            {
                if (child->type != XML_ELEMENT_NODE)
                    continue;

                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                const std::string elem_name = reinterpret_cast<const char*>(child->name);
                if (elem_name == "DirectDependency")
                {
                    dd_node = child;
                    break;
                }
            }

            if (dd_node != nullptr)
            {
                for (xmlNodePtr name_node = dd_node->children; name_node != nullptr; name_node = name_node->next)
                {
                    if (name_node->type != XML_ELEMENT_NODE)
                        continue;

                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                    const std::string node_name = reinterpret_cast<const char*>(name_node->name);
                    if (node_name == "Name")
                    {
                        xmlChar* content = xmlNodeGetContent(name_node);
                        if (content != nullptr)
                        {
                            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                            const std::string dep_name = reinterpret_cast<const char*>(content);
                            xmlFree(content);

                            auto it = variable_map.find(dep_name);
                            if (it == variable_map.end())
                            {
                                test.setStatus(TestStatus::FAIL);
                                test.getMessages().emplace_back(
                                    std::format("Variable '{}' (line {}) references non-existent variable '{}' in "
                                                "DirectDependency.",
                                                var_name.value_or("unknown"), name_node->line, dep_name));
                            }
                            else if (it->second->causality != "input")
                            {
                                test.setStatus(TestStatus::FAIL);
                                test.getMessages().emplace_back(std::format(
                                    "Variable '{}' (line {}) references variable '{}' in DirectDependency "
                                    "which exists but is not an input (causality='{}').",
                                    var_name.value_or("unknown"), name_node->line, dep_name, it->second->causality));
                            }
                        }
                    }
                }
            }
        }
    }

    if (xpath_obj != nullptr)
        xmlXPathFreeObject(xpath_obj);

    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::checkGenerationDateReleaseYear(const std::string& dt, std::time_t generation_time,
                                                                 TestResult& test) const
{
    checkGenerationDateReleaseYearBase(dt, generation_time, 2010, "1.0", test);
}

void Fmi1ModelDescriptionChecker::checkAnnotations(xmlDocPtr doc, Certificate& cert) const
{
    TestResult test{"Vendor Annotations Uniqueness", TestStatus::PASS, {}};
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/VendorAnnotations/Tool");
    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
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
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format(
                        "Vendor annotation tool '{}' (line {}) is defined multiple times.)", *name, node->line));
                }
                seen_names.insert(*name);
            }
        }
    }
    if (xpath_obj != nullptr)
        xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::applyDefaultInitialValues(std::vector<Variable>& variables) const
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
                                                                               Certificate& cert) const
{
    TestResult test{"Causality/Variability/Initial Combinations", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        if (var.variability == "constant")
        {
            if (var.causality == "input")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "Variable '{}' (line {}) has illegal combination: variability='constant' and causality='input'. "
                    "Logical contradiction: constants cannot be changed from the outside.",
                    var.name, var.sourceline));
            }
        }
    }
    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert) const
{
    TestResult test{"Legal Variability", TestStatus::PASS, {}};
    for (const auto& var : variables)
    {
        // Only Real can be continuous
        if (var.variability == "continuous")
        {
            if (var.type != "Real")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Variable '{}' (line {}) is of type {} and cannot have variability 'continuous'.",
                                var.name, var.sourceline, var.type));
            }
        }
    }
    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::checkRequiredStartValues(const std::vector<Variable>& variables,
                                                           Certificate& cert) const
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

        if (needs_start && !var.start)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Variable '{}' (line {}) must have a start value.", var.name, var.sourceline));
        }
    }
    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::checkIllegalStartValues(const std::vector<Variable>& variables,
                                                          Certificate& cert) const
{
    TestResult test{"Illegal Start Values", TestStatus::PASS, {}};
    for (const auto& var : variables)
    {
        // FMI 1.0: "fixed" attribute is only allowed if "start" is present
        if (var.fixed && !var.start)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Variable '{}' (line {}) has 'fixed' attribute but is missing 'start' value.", var.name,
                            var.sourceline));
        }

        // FMI 1.0: "fixed" attribute is not defined for causality="input" or "none"
        if ((var.causality == "input" || var.causality == "none") && var.fixed)
        {
            test.setStatus(TestStatus::FAIL);
            if (var.causality == "input")
            {
                test.getMessages().emplace_back(std::format(
                    "Variable '{}' (line {}) has causality='input' and a 'fixed' attribute. The 'fixed' attribute is "
                    "only defined for causalities other than 'input' (Section 3.3).",
                    var.name, var.sourceline));
            }
            else
            {
                test.getMessages().emplace_back(std::format(
                    "Variable '{}' (line {}) has attribute 'fixed' but causality='none' — this variable does not "
                    "participate in initialization and 'fixed' has no meaning here.",
                    var.name, var.sourceline));
            }
        }

        // FMI 1.0: "fixed" attribute for variability="constant"
        if (var.variability == "constant" && var.fixed && !*var.fixed)
        {
            // fixed="false" (guess value) makes no sense for a constant
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format(
                "Variable '{}' (line {}) has variability='constant' and fixed='false', which is a contradiction.",
                var.name, var.sourceline));
        }
    }
    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::checkMinMaxStartValues(const std::vector<Variable>& variables,
                                                         const std::map<std::string, TypeDefinition>& type_definitions,
                                                         Certificate& cert) const
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
    xmlDocPtr doc, [[maybe_unused]] const std::vector<std::string>& interface_elements) const
{
    std::map<std::string, std::string> model_identifiers;
    xmlNodePtr root = xmlDocGetRootElement(doc);
    auto model_id = getXmlAttribute(root, "modelIdentifier");
    if (model_id)
    {
        bool is_cs = false;
        xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/Implementation");
        if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr && xpath_obj->nodesetval->nodeNr > 0)
            is_cs = true;
        if (xpath_obj != nullptr)
            xmlXPathFreeObject(xpath_obj);

        if (is_cs)
            model_identifiers["CoSimulation"] = *model_id;
        else
            model_identifiers["ModelExchange"] = *model_id;
    }
    return model_identifiers;
}

ModelMetadata Fmi1ModelDescriptionChecker::extractMetadata(xmlNodePtr root) const
{
    ModelMetadata metadata;
    metadata.fmi_version = getXmlAttribute(root, "fmiVersion");
    metadata.model_name = getXmlAttribute(root, "modelName");
    metadata.guid = getXmlAttribute(root, "guid");
    metadata.model_version = getXmlAttribute(root, "version");
    metadata.author = getXmlAttribute(root, "author");
    metadata.description = getXmlAttribute(root, "description");
    metadata.generation_tool = getXmlAttribute(root, "generationTool");
    metadata.generation_date_and_time = getXmlAttribute(root, "generationDateAndTime");
    metadata.variable_naming_convention = getXmlAttribute(root, "variableNamingConvention").value_or("flat");

    auto num_event_ind = getXmlAttribute(root, "numberOfEventIndicators");
    if (num_event_ind)
        metadata.number_of_event_indicators = parseNumber<uint32_t>(*num_event_ind);
    return metadata;
}

std::map<std::string, UnitDefinition> Fmi1ModelDescriptionChecker::extractUnitDefinitions(xmlDocPtr doc) const
{
    std::map<std::string, UnitDefinition> units;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/UnitDefinitions/BaseUnit");
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
        unit_def.name = getXmlAttribute(unit_node, "unit").value_or("");
        unit_def.sourceline = unit_node->line;
        if (unit_def.name.empty())
            continue;

        for (xmlNodePtr child = unit_node->children; child != nullptr; child = child->next)
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

std::map<std::string, TypeDefinition> Fmi1ModelDescriptionChecker::extractTypeDefinitions(xmlDocPtr doc) const
{
    std::map<std::string, TypeDefinition> type_definitions;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/TypeDefinitions/Type");
    if (xpath_obj == nullptr || xpath_obj->nodesetval == nullptr)
    {
        if (xpath_obj != nullptr)
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

        for (xmlNodePtr child = type_node->children; child != nullptr; child = child->next)
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

std::vector<Variable> Fmi1ModelDescriptionChecker::extractVariables(xmlDocPtr doc) const
{
    _is_cs = false;
    xmlXPathObjectPtr xpath_obj_impl = getXPathNodes(doc, "/fmiModelDescription/Implementation");
    if (xpath_obj_impl != nullptr && xpath_obj_impl->nodesetval != nullptr && xpath_obj_impl->nodesetval->nodeNr > 0)
        _is_cs = true;
    if (xpath_obj_impl != nullptr)
        xmlXPathFreeObject(xpath_obj_impl);

    std::vector<Variable> variables;
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/ModelVariables/ScalarVariable");
    if (xpath_obj == nullptr || xpath_obj->nodesetval == nullptr)
    {
        if (xpath_obj != nullptr)
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
                    var.initial = (*var.fixed ? "exact" : "approx");
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

void Fmi1ModelDescriptionChecker::checkUnits(xmlDocPtr doc, Certificate& cert) const
{
    TestResult test{"Unit Definitions", TestStatus::PASS, {}};
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/UnitDefinitions/BaseUnit");
    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
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
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(
                        std::format("Unit '{}' (line {}) is defined multiple times.", *name, node->line));
                }
                seen_names.insert(*name);
            }
        }
    }
    if (xpath_obj != nullptr)
        xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::checkTypeDefinitions(xmlDocPtr doc, Certificate& cert) const
{
    TestResult test{"Type Definitions", TestStatus::PASS, {}};
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/TypeDefinitions/Type");
    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
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
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(
                        std::format("Type definition '{}' (line {}) is defined multiple times.", *name, node->line));
                }
                seen_names.insert(*name);
            }
        }
    }
    if (xpath_obj != nullptr)
        xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::validateVariableSpecialFloat(TestResult& /*test*/, const Variable& /*var*/,
                                                               const std::string& /*val*/,
                                                               const std::string& /*attr_name*/) const
{
    // Special floats are allowed in FMI 1.0
}

void Fmi1ModelDescriptionChecker::validateDefaultExperimentSpecialFloat(TestResult& /*test*/,
                                                                        const std::string& /*val*/,
                                                                        const std::string& /*attr_name*/) const
{
    // Special floats are allowed in FMI 1.0
}

void Fmi1ModelDescriptionChecker::validateUnitSpecialFloat(TestResult& /*test*/, const std::string& /*val*/,
                                                           const std::string& /*attr_name*/,
                                                           const std::string& /*unit_name*/, size_t /*line*/) const
{
    // Special floats are allowed in FMI 1.0
}

void Fmi1ModelDescriptionChecker::validateTypeDefinitionSpecialFloat(TestResult& /*test*/,
                                                                     const TypeDefinition& /*type_def*/,
                                                                     const std::string& /*val*/,
                                                                     const std::string& /*attr_name*/) const
{
    // Special floats are allowed in FMI 1.0
}

void Fmi1ModelDescriptionChecker::checkModelIdentifier(const std::string& model_identifier,
                                                       const std::string& interface_name, Certificate& cert) const
{
    ModelDescriptionCheckerBase::checkModelIdentifier(model_identifier, interface_name, cert);
}

void Fmi1ModelDescriptionChecker::checkImplementation(xmlDocPtr doc, Certificate& cert) const
{
    // For FMI 1.0, the presence of the Implementation element distinguishes Co-Simulation from Model Exchange.
    // If present, we validate its contents (CoSimulation_StandAlone or CoSimulation_Tool).
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "/fmiModelDescription/Implementation");
    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr && xpath_obj->nodesetval->nodeNr > 0)
    {
        TestResult test{"CS Implementation", TestStatus::PASS, {}};
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        xmlNodePtr impl_node = xpath_obj->nodesetval->nodeTab[0];
        xmlNodePtr tool_node = nullptr;

        for (xmlNodePtr child = impl_node->children; child != nullptr; child = child->next)
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

        if (tool_node != nullptr)
        {
            // For CoSimulation_Tool, check Model element and its attributes
            xmlNodePtr model_node = nullptr;
            for (xmlNodePtr child = tool_node->children; child != nullptr; child = child->next)
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

            if (model_node != nullptr)
            {
                auto entry_point = getXmlAttribute(model_node, "entryPoint");
                if (entry_point)
                    checkUri(*entry_point, "entryPoint", model_node->line, test);

                // Check additional files
                for (xmlNodePtr child = model_node->children; child != nullptr; child = child->next)
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
            for (xmlNodePtr child = impl_node->children; child != nullptr; child = child->next)
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
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    "Implementation element must contain either CoSimulation_StandAlone or CoSimulation_Tool.");
            }
        }

        cert.printTestResult(test);
    }
    if (xpath_obj != nullptr)
        xmlXPathFreeObject(xpath_obj);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Fmi1ModelDescriptionChecker::checkUri(const std::string& uri, const std::string& attr_name, int line,
                                           TestResult& test) const
{
    if (uri.starts_with("fmu://"))
    {
        std::string relative_path = uri.substr(6); // Remove "fmu://"
        // The path in fmu:// scheme is relative to the FMU root.
        // We remove any leading slash if present after fmu:// (e.g., fmu:///resources/...)
        if (!relative_path.empty() && relative_path[0] == '/')
            relative_path.erase(0, 1);

        const std::filesystem::path full_path = getFmuRootPath() / relative_path;
        if (!std::filesystem::exists(full_path))
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format("Attribute '{}' (line {}) references missing file in FMU: '{}'",
                                                        attr_name, line, relative_path));
        }
    }
}

void Fmi1ModelDescriptionChecker::checkAliases(const std::vector<Variable>& variables, Certificate& cert) const
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
        if (var.value_reference)
            alias_sets[{get_base_type(var.type), *var.value_reference}].push_back(&var);

    TestResult variability_consistency_test{"Alias Variability Consistency", TestStatus::PASS, {}};

    for (const auto& [key, alias_set] : alias_sets)
    {
        if (alias_set.size() <= 1)
            continue;

        const auto& [base_type, vr] = key;
        const Variable* first = alias_set[0];
        const Variable* first_with_start = nullptr;
        double first_normalized_start = 0.0;

        int no_alias_count = 0;
        const Variable* constant_var = nullptr;
        const Variable* non_constant_var = nullptr;
        bool variability_mismatch = false;

        for (const auto* var : alias_set)
        {
            // negatedAlias check
            const bool negated = (var->alias && *var->alias == "negatedAlias");
            if (negated && base_type != "Real" && base_type != "Integer/Enumeration")
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "Variable '{}' (line {}) has alias='negatedAlias' but is of type {}. Only Real and Integer "
                    "variables can be negated.",
                    var->name, var->sourceline, var->type));
            }

            // noAlias count
            if (!var->alias || *var->alias == "noAlias")
                no_alias_count++;

            // variability
            if (var->variability == "constant")
                constant_var = var;
            else
                non_constant_var = var;

            if (var->variability != first->variability)
                variability_mismatch = true;

            // 1. Same base type
            if (var->type != first->type)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("All variables in an alias set (VR {}) must have the same type. "
                                "Variable '{}' is {} but '{}' is {}.",
                                vr, var->name, var->type, first->name, first->type));
            }

            // 2. If Real, same unit
            if (base_type == "Real" && var->unit != first->unit)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "All variables in an alias set (VR {}) must have the same unit. Variable '{}' has "
                    "unit '{}' but '{}' has unit '{}'.",
                    vr, var->name, var->unit.value_or("(none)"), first->name, first->unit.value_or("(none)")));
            }

            // 3. Equivalent start values
            if (var->start)
            {
                double current_val = 0;
                bool valid = false;

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
                            test.setStatus(TestStatus::FAIL);

                            const bool first_negated =
                                (first_with_start->alias && *first_with_start->alias == "negatedAlias");

                            std::string msg1 = std::format("'{}' (", var->name);
                            if (negated)
                                msg1 += "negated, ";
                            if (var->start)
                                msg1 += std::format("start='{}'", *var->start);

                            std::string msg2 = std::format("'{}' (", first_with_start->name);
                            if (first_negated)
                                msg2 += "negated, ";
                            if (first_with_start->start)
                                msg2 += std::format("start='{}'", *first_with_start->start);

                            test.getMessages().emplace_back(std::format(
                                "All variables in an alias set (VR {}) must have equivalent start values. {} and {} "
                                "are inconsistent.",
                                vr, msg1, msg2));
                        }
                    }
                }
            }
        }

        if (no_alias_count > 1)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("All variables in an alias set (VR {}) must have exactly one base variable (noAlias). "
                            "Multiple base variables found.",
                            vr));
        }
        else if (no_alias_count == 0)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format(
                "All variables in an alias set (VR {}) must have exactly one base variable (noAlias). No base variable "
                "found.",
                vr));
        }

        if (constant_var && non_constant_var)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format(
                "All variables in an alias set (VR {}) must have the same variability. Variable '{}' is constant "
                "but '{}' is {}. Constants can only be aliased to other constants.",
                vr, constant_var->name, non_constant_var->name, non_constant_var->variability));
        }
        else if (variability_mismatch)
        {
            const Variable* mismatch = nullptr;
            for (const auto* var : alias_set)
            {
                if (var->variability != first->variability)
                {
                    mismatch = var;
                    break;
                }
            }

            variability_consistency_test.setStatus(TestStatus::WARNING);
            variability_consistency_test.getMessages().emplace_back(
                std::format("All variables in an alias set (VR {}) should have the same variability. Variable '{}' "
                            "is {} but '{}' is {}.",
                            vr, first->name, first->variability, mismatch->name, mismatch->variability));
        }
    }

    if (variability_consistency_test.getStatus() != TestStatus::PASS)
        cert.printTestResult(variability_consistency_test);

    cert.printTestResult(test);
}
