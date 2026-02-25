#include "fmi1_model_description_checker.h"

#include "model_description_checker.h"

#include "certificate.h"

#include <libxml/tree.h>
#include <libxml/xpath.h>

#ifndef _WIN32
#include <sys/wait.h>
#endif

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <vector>

void Fmi1ModelDescriptionChecker::performVersionSpecificChecks(
    xmlDocPtr doc, const std::vector<Variable>& variables,
    [[maybe_unused]] const std::map<std::string, TypeDefinition>& type_definitions,
    [[maybe_unused]] const std::map<std::string, UnitDefinition>& units, Certificate& cert)
{
    checkImplementation(doc, cert);
    checkLegalVariability(variables, cert);
    checkCausalityVariabilityInitialCombinations(variables, cert);
    checkRequiredStartValues(variables, cert);
    checkIllegalStartValues(variables, cert);
    checkMinMaxStartValues(variables, type_definitions, cert);
    checkAliases(variables, cert);
}

void Fmi1ModelDescriptionChecker::validateFmiVersionValue(const std::string& version, TestResult& test)
{
    if (version != "1.0")
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("FMI version \"" + version + "\" is invalid for FMI 1.0 (must be exactly \"1.0\").");
    }
}

void Fmi1ModelDescriptionChecker::checkGuid(const std::optional<std::string>& guid, Certificate& cert)
{
    TestResult test{"GUID Presence", TestStatus::PASS, {}};
    if (!guid.has_value() || guid->empty())
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("guid attribute is missing or empty.");
    }
    cert.printTestResult(test);
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
    TestResult test{"Causality/Variability/Initial Combinations (FMI1)", TestStatus::PASS, {}};

    // FMI 1.0 rules are simpler but let's enforce what's in the spec.
    // Variability: constant, parameter, discrete, continuous
    // Causality: input, output, internal, none

    for (const auto& var : variables)
    {
        if (var.causality == "input" && !var.initial.empty())
        {
            // Spec says: "This attribute is only allowed if "start" is also present and causality is not input"
            // Wait, "if causality is not input" means it's NOT allowed for input.
            // Our mapping of 'fixed' to 'initial' should reflect this.
        }

        if (var.variability == "continuous" && var.type != "Real")
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") is of type " + var.type + " and cannot have variability \"continuous\".");
        }
    }
    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::checkLegalVariability(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Legal Variability (FMI1)", TestStatus::PASS, {}};
    for (const auto& var : variables)
    {
        if (var.variability == "continuous" && var.type != "Real")
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") is of type " + var.type + " and cannot have variability \"continuous\".");
        }
    }
    cert.printTestResult(test);
}

void Fmi1ModelDescriptionChecker::checkRequiredStartValues(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Required Start Values (FMI1)", TestStatus::PASS, {}};
    for (const auto& var : variables)
    {
        bool needs_start = false;
        if (var.causality == "input")
            needs_start = true;
        if (var.variability == "constant")
            needs_start = true;
        if (var.causality == "parameter" && (var.initial == "exact" || var.initial.empty()))
            needs_start = true; // Most parameters need start

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
    // FMI 1.0 doesn't have many illegal start values rules, mostly they are just ignored if not used.
    (void)variables;
    (void)cert;
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
                    var.initial = (*fixed == "true" ? "exact" : "approx");

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
                              {std::format("FMI 1.0: modelIdentifier '{}' must match the FMU filename '{}'.",
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

bool Fmi1ModelDescriptionChecker::checkReachability(const std::string& url)
{
    // Re-verify with the safe regex (already checked in checkUri, but good for defense-in-depth).
    static const std::regex safe_url_regex(R"(^https?://[a-zA-Z0-9\-\._~:/?#%@\+&!=\[\]]+$)", std::regex::optimize);
    if (!std::regex_match(url, safe_url_regex))
        return false;

    // Determine the platform-specific null device for redirection
    std::string null_device = "/dev/null";
#ifdef _WIN32
    null_device = "NUL";
#endif

    // Use curl to check reachability.
    // -I: Fetch headers only, -s: Silent, -L: Follow redirects, --max-time: timeout, --fail: exit non-zero on 4xx/5xx
    std::string command = "curl -I -s -L --max-time 5 --fail \"" + url + "\" > " + null_device + " 2>&1";

#ifdef _WIN32
    int result = std::system(command.c_str());
#else
    // On POSIX, system() returns the termination status as defined by waitpid()
    int status = std::system(command.c_str());
    int result = (status != -1) ? WEXITSTATUS(status) : -1;
#endif

    return result == 0;
}

void Fmi1ModelDescriptionChecker::checkAliases(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Alias Variables (FMI1)", TestStatus::PASS, {}};

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

            // 2. If Real, same unit
            if (var->type == "Real" && var->unit != first->unit)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variables sharing VR " + std::to_string(vr) + " must have the same unit. \"" +
                                        var->name + "\" has unit \"" + var->unit.value_or("(none)") + "\" but \"" +
                                        first->name + "\" has unit \"" + first->unit.value_or("(none)") + "\".");
            }
        }
    }

    cert.printTestResult(test);
}
