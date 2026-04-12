#include "terminals_and_icons_checker.h"

#include "certificate.h"
#include "file_utils.h"
#include "xml_utils.h"

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>

void TerminalsAndIconsCheckerBase::validate(const std::filesystem::path& path, Certificate& cert) const
{
    const auto terminals_path = path / "terminalsAndIcons" / "terminalsAndIcons.xml";
    if (!std::filesystem::exists(terminals_path))
        return;

    cert.printSubsectionHeader("TERMINALS AND ICONS VALIDATION");

    std::string fmiModelDescriptionVersion;
    const auto variables = extractVariables(path, cert, fmiModelDescriptionVersion);

    if (fmiModelDescriptionVersion.empty())
    {
        cert.printSubsectionSummary(false);
        return;
    }

    checkTerminalsAndIcons(path, variables, cert);

    cert.printSubsectionSummary(true);
}

std::optional<std::string> TerminalsAndIconsCheckerBase::getXmlAttribute(xmlNodePtr node,
                                                                         const std::string& attr_name) const
{
    if (node == nullptr)
        return std::nullopt;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    xmlChar* attr = xmlGetProp(node, reinterpret_cast<const xmlChar*>(attr_name.c_str()));
    if (attr == nullptr)
        return std::nullopt;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    std::string value(reinterpret_cast<char*>(attr));
    xmlFree(attr);
    return value;
}

bool TerminalsAndIconsCheckerBase::checkTerminalsAndIcons(const std::filesystem::path& path,
                                                          const std::map<std::string, TerminalVariableInfo>& variables,
                                                          Certificate& cert) const
{
    auto terminals_path = path / "terminalsAndIcons" / "terminalsAndIcons.xml";

    xmlDocPtr doc = readXmlFile(terminals_path);
    if (!doc)
    {
        const TestResult test{
            "Parse terminalsAndIcons.xml", TestStatus::FAIL, {"Failed to parse terminalsAndIcons.xml."}};
        cert.printTestResult(test);
        return false;
    }

    bool all_passed = true;
    const auto print_test = [&](const TestResult& test)
    {
        if (test.getStatus() == TestStatus::FAIL)
            all_passed = false;
        cert.printTestResult(test);
    };

    const xmlNodePtr root = xmlDocGetRootElement(doc);
    const xmlXPathContextPtr context = xmlXPathNewContext(doc);
    std::string prefix = "";
    if (root != nullptr && root->ns != nullptr && root->ns->href != nullptr)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        xmlXPathRegisterNs(context, reinterpret_cast<const xmlChar*>("f"), root->ns->href);
        prefix = "f:";
    }

    // 1. Check fmi_version consistency
    {
        TestResult test{"Terminals and Icons Version", TestStatus::PASS, {}};
        checkFmiVersion(root, test);
        print_test(test);
    }

    // 2. Check uniqueness of terminal names on each level
    {
        TestResult test{"Unique Terminal Names", TestStatus::PASS, {}};
        checkUniqueTerminalNames(context, prefix, test);
        print_test(test);
    }

    // 3. Variable references and constraints
    {
        TestResult test{"Terminal Member Variables", TestStatus::PASS, {}};
        checkVariableReferences(context, prefix, variables, test);
        print_test(test);
    }

    // 4. Member name uniqueness within a terminal
    {
        TestResult test{"Unique Member Names", TestStatus::PASS, {}};
        checkUniqueMemberNames(context, prefix, test);
        print_test(test);
    }

    // 5. Stream and inflow/outflow constraint
    {
        TestResult test{"Stream and Flow Constraints", TestStatus::PASS, {}};
        checkStreamFlowConstraints(context, prefix, test);
        print_test(test);
    }

    // 6. Graphical Representation
    {
        TestResult test{"Graphical Representation", TestStatus::PASS, {}};
        checkGraphicalRepresentation(path, context, prefix, test);
        print_test(test);
    }

    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);
    return all_passed;
}

void TerminalsAndIconsCheckerBase::checkUniqueTerminalNames(xmlXPathContextPtr context, const std::string& prefix,
                                                            TestResult& test) const
{
    const auto check_unique_terminals = [&](auto self, const xmlNodePtr parent) -> void
    {
        std::set<std::string> seen_names;
        for (xmlNodePtr child = parent->children; child != nullptr; child = child->next)
        {
            if (child->type == XML_ELEMENT_NODE &&
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                xmlStrcmp(child->name, reinterpret_cast<const xmlChar*>("Terminal")) == 0)
            {
                const auto name = getXmlAttribute(child, "name");
                if (name.has_value())
                {
                    const auto& val = *name;
                    if (seen_names.contains(val))
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back("Terminal name '" + val + "' (line " +
                                                        std::to_string(child->line) + ") is not unique at its level.");
                    }
                    seen_names.insert(val);
                }
                self(self, child);
            }
        }
    };

    const std::string expr = "/" + prefix + "fmiTerminalsAndIcons/" + prefix + "Terminals";

    const xmlXPathObjectPtr terminals_elem =
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.c_str()), context);
    if (terminals_elem != nullptr && terminals_elem->nodesetval != nullptr && terminals_elem->nodesetval->nodeNr > 0)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        check_unique_terminals(check_unique_terminals, terminals_elem->nodesetval->nodeTab[0]);
    }
    if (terminals_elem != nullptr)
        xmlXPathFreeObject(terminals_elem);
}

void TerminalsAndIconsCheckerBase::checkVariableReferences(xmlXPathContextPtr context, const std::string& prefix,
                                                           const std::map<std::string, TerminalVariableInfo>& variables,
                                                           TestResult& test) const
{
    std::string expr = "//" + prefix + "TerminalMemberVariable";

    const xmlXPathObjectPtr member_vars =
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.c_str()), context);
    if (member_vars != nullptr && member_vars->nodesetval != nullptr)
    {
        for (int i = 0; i < member_vars->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = member_vars->nodesetval->nodeTab[i];
            const auto var_name = getXmlAttribute(node, "variableName");
            const auto var_kind = getXmlAttribute(node, "variableKind");

            if (var_name.has_value())
            {
                const auto& name_val = *var_name;
                const auto it = variables.find(name_val);
                if (it == variables.end())
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back("TerminalMemberVariable (line " + std::to_string(node->line) +
                                                    ") references non-existent variable '" + name_val + "'.");
                }
                else
                {
                    const auto& var_info = it->second;
                    if (var_kind.has_value())
                    {
                        const auto& kind_val = *var_kind;
                        if (kind_val == "signal" || kind_val == "inflow" || kind_val == "outflow")
                        {
                            if (var_info.causality != "input" && var_info.causality != "output" &&
                                var_info.causality != "parameter" && var_info.causality != "calculatedParameter")
                            {
                                test.setStatus(TestStatus::FAIL);
                                test.getMessages().emplace_back(
                                    std::format("Variable '{}' used as '{}' in terminal (line {}): must have "
                                                "causality 'input', 'output', 'parameter', or 'calculatedParameter'.",
                                                name_val, kind_val, node->line));
                            }
                        }
                    }
                }
            }
        }
    }
    if (member_vars != nullptr)
        xmlXPathFreeObject(member_vars);

    expr = "//" + prefix + "TerminalStreamMemberVariable";
    const xmlXPathObjectPtr stream_vars =
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.c_str()), context);
    if (stream_vars != nullptr && stream_vars->nodesetval != nullptr)
    {
        for (int i = 0; i < stream_vars->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = stream_vars->nodesetval->nodeTab[i];
            const auto in_stream = getXmlAttribute(node, "inStreamVariableName");
            const auto out_stream = getXmlAttribute(node, "outStreamVariableName");

            if (in_stream.has_value())
            {
                const auto& in_val = *in_stream;
                const auto it = variables.find(in_val);
                if (it == variables.end())
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back("TerminalStreamMemberVariable (line " + std::to_string(node->line) +
                                                    ") references non-existent variable '" + in_val +
                                                    "' in inStreamVariableName.");
                }
                else if (it->second.causality != "input" && it->second.causality != "parameter")
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back("Variable '" + in_val + "' used in inStreamVariableName (line " +
                                                    std::to_string(node->line) +
                                                    ") must have causality 'input' or 'parameter'.");
                }
            }

            if (in_stream.has_value() && out_stream.has_value())
            {
                const auto it_in = variables.find(*in_stream);
                const auto it_out = variables.find(*out_stream);

                if (it_in != variables.end() && it_out != variables.end())
                {
                    if (it_in->second.dimensions != it_out->second.dimensions)
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(
                            "TerminalStreamMemberVariable (line " + std::to_string(node->line) +
                            ") has mismatched dimensions for '" + *in_stream + "' and '" + *out_stream + "'.");
                    }
                }
            }

            if (out_stream.has_value())
            {
                const auto& out_val = *out_stream;
                const auto it = variables.find(out_val);
                if (it == variables.end())
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back("TerminalStreamMemberVariable (line " + std::to_string(node->line) +
                                                    ") references non-existent variable '" + out_val +
                                                    "' in outStreamVariableName.");
                }
                else if (it->second.causality != "output" && it->second.causality != "calculatedParameter")
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back("Variable '" + out_val + "' used in outStreamVariableName (line " +
                                                    std::to_string(node->line) +
                                                    ") must have causality 'output' or 'calculatedParameter'.");
                }
            }
        }
    }
    if (stream_vars != nullptr)
        xmlXPathFreeObject(stream_vars);
}

void TerminalsAndIconsCheckerBase::checkUniqueMemberNames(xmlXPathContextPtr context, const std::string& prefix,
                                                          TestResult& test) const
{
    const auto check_unique_members = [&](auto self, const xmlNodePtr terminal) -> void
    {
        std::set<std::string> seen_members;
        const auto matching_rule = getXmlAttribute(terminal, "matchingRule").value_or("plug");

        for (xmlNodePtr child = terminal->children; child != nullptr; child = child->next)
        {
            if (child->type == XML_ELEMENT_NODE)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                const std::string elem_name = reinterpret_cast<const char*>(child->name);
                if (elem_name == "TerminalMemberVariable")
                {
                    const auto member_name = getXmlAttribute(child, "memberName");
                    if (member_name.has_value())
                    {
                        const auto& val = *member_name;
                        if (seen_members.contains(val))
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back("Member name '" + val +
                                                            "' is not unique in terminal (line " +
                                                            std::to_string(child->line) + ").");
                        }
                        seen_members.insert(val);
                    }
                    else if (matching_rule == "plug" || matching_rule == "bus")
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back(std::format(
                            "TerminalMemberVariable (line {}) is missing mandatory 'memberName' for matchingRule '{}'.",
                            child->line, matching_rule));
                    }
                }
                else if (elem_name == "TerminalStreamMemberVariable")
                {
                    const auto in_member = getXmlAttribute(child, "inStreamMemberName");
                    const auto out_member = getXmlAttribute(child, "outStreamMemberName");
                    if (in_member.has_value())
                    {
                        const auto& in_val = *in_member;
                        if (seen_members.contains(in_val))
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back("Stream member name '" + in_val +
                                                            "' is not unique in terminal (line " +
                                                            std::to_string(child->line) + ").");
                        }
                        seen_members.insert(in_val);
                    }
                    if (out_member.has_value() && (!in_member.has_value() || *out_member != *in_member))
                    {
                        const auto& out_val = *out_member;
                        if (seen_members.contains(out_val))
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back("Stream member name '" + out_val +
                                                            "' is not unique in terminal (line " +
                                                            std::to_string(child->line) + ").");
                        }
                        seen_members.insert(out_val);
                    }
                }
                else if (elem_name == "Terminal")
                {
                    self(self, child);
                }
            }
        }
    };

    const std::string expr = "/" + prefix + "fmiTerminalsAndIcons/" + prefix + "Terminals//" + prefix + "Terminal";

    const xmlXPathObjectPtr terminals_elem =
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.c_str()), context);
    if (terminals_elem != nullptr && terminals_elem->nodesetval != nullptr)
    {
        for (int i = 0; i < terminals_elem->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            check_unique_members(check_unique_members, terminals_elem->nodesetval->nodeTab[i]);
        }
    }
    if (terminals_elem != nullptr)
        xmlXPathFreeObject(terminals_elem);
}

void TerminalsAndIconsCheckerBase::checkGraphicalRepresentation(const std::filesystem::path& path,
                                                                xmlXPathContextPtr context, const std::string& prefix,
                                                                TestResult& test) const
{
    const std::string expr = "//" + prefix + "TerminalGraphicalRepresentation";

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const xmlXPathObjectPtr xpath_obj = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.c_str()), context);

    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        for (int i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            const auto icon_base = getXmlAttribute(node, "iconBaseName");

            if (icon_base.has_value())
            {
                const auto& base_val = *icon_base;
                if (base_val.find("..") != std::string::npos || base_val.starts_with("/") ||
                    base_val.find(":") != std::string::npos)
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(
                        "iconBaseName '" + base_val + "' (line " + std::to_string(node->line) +
                        ") must be a relative URI and must not contain '..', ':' or start with "
                        "'/'.");
                }
                else
                {
                    // PNG file must be provided
                    const auto png_path = path / "terminalsAndIcons" / (base_val + ".png");
                    if (!std::filesystem::exists(png_path))
                    {
                        test.setStatus(TestStatus::FAIL);
                        test.getMessages().emplace_back("Terminal icon PNG file '" + file_utils::pathToUtf8(png_path) +
                                                        "' (referenced line " + std::to_string(node->line) +
                                                        ") not found.");
                    }
                    else
                    {
                        if (!file_utils::hasPngMagic(png_path))
                        {
                            test.setStatus(TestStatus::FAIL);
                            test.getMessages().emplace_back(std::format(
                                "File '{}' (referenced line {}) is not a valid PNG image (invalid magic bytes).",
                                std::string("terminalsAndIcons/") + file_utils::pathToUtf8(png_path.filename()),
                                node->line));
                        }
                        else
                        {
                            const auto dimensions = file_utils::getPngDimensions(png_path);
                            if (dimensions.has_value())
                            {
                                if (dimensions->first < 100 || dimensions->second < 100)
                                {
                                    if (test.getStatus() != TestStatus::FAIL)
                                        test.setStatus(TestStatus::WARNING);
                                    test.getMessages().emplace_back(std::format(
                                        "Terminal icon '{}' (referenced line {}) is small ({}x{} pixels). A size of at "
                                        "least 100x100 pixels is recommended.",
                                        file_utils::pathToUtf8(png_path.filename()), node->line, dimensions->first,
                                        dimensions->second));
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "TerminalGraphicalRepresentation (line {}) is missing mandatory 'iconBaseName'.", node->line));
            }

            const auto connection_color = getXmlAttribute(node, "defaultConnectionColor");
            if (connection_color.has_value())
            {
                const auto& color_val = *connection_color;
                std::stringstream ss(color_val);
                std::string part;
                int count = 0;
                while (ss >> part)
                    count++;
                if (count != 3)
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back("defaultConnectionColor '" + color_val + "' (line " +
                                                    std::to_string(node->line) + ") must have exactly 3 RGB values.");
                }
            }
        }
    }

    if (xpath_obj != nullptr)
        xmlXPathFreeObject(xpath_obj);
}

void TerminalsAndIconsCheckerBase::checkStreamFlowConstraints(xmlXPathContextPtr context, const std::string& prefix,
                                                              TestResult& test) const
{
    const auto check_stream_flow_constraint = [&](auto self, const xmlNodePtr terminal) -> void
    {
        bool has_stream = false;
        int flow_count = 0;

        for (xmlNodePtr child = terminal->children; child != nullptr; child = child->next)
        {
            if (child->type == XML_ELEMENT_NODE)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                const std::string elem_name = reinterpret_cast<const char*>(child->name);
                if (elem_name == "TerminalStreamMemberVariable")
                {
                    has_stream = true;
                }
                else if (elem_name == "TerminalMemberVariable")
                {
                    const auto var_kind = getXmlAttribute(child, "variableKind");
                    if (var_kind.has_value())
                    {
                        const auto& kind_val = *var_kind;
                        if (kind_val == "inflow" || kind_val == "outflow")
                            flow_count++;
                    }
                }
                else if (elem_name == "Terminal")
                {
                    self(self, child);
                }
            }
        }

        if (has_stream && flow_count > 1)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back("Terminal '" + getXmlAttribute(terminal, "name").value_or("unnamed") +
                                            "' (line " + std::to_string(terminal->line) +
                                            ") has multiple inflow/outflow variables and a stream variable. Only one "
                                            "inflow/outflow variable is allowed when a stream variable is present.");
        }
    };

    const std::string expr = "/" + prefix + "fmiTerminalsAndIcons/" + prefix + "Terminals//" + prefix + "Terminal";

    const xmlXPathObjectPtr terminals_elem =
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.c_str()), context);
    if (terminals_elem != nullptr && terminals_elem->nodesetval != nullptr)
    {
        for (int i = 0; i < terminals_elem->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            check_stream_flow_constraint(check_stream_flow_constraint, terminals_elem->nodesetval->nodeTab[i]);
        }
    }
    if (terminals_elem != nullptr)
        xmlXPathFreeObject(terminals_elem);
}
