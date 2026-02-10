#include "terminals_and_icons_checker.h"
#include "certificate.h"
#include <algorithm>
#include <iostream>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <set>
#include <sstream>

void TerminalsAndIconsCheckerBase::validate(const std::filesystem::path& path, Certificate& cert)
{
    cert.printSubsectionHeader("TERMINALS AND ICONS VALIDATION");

    std::string fmiModelDescriptionVersion;
    auto variables = extractVariables(path, cert, fmiModelDescriptionVersion);

    if (fmiModelDescriptionVersion.empty())
    {
        cert.printSubsectionSummary(false);
        return;
    }

    bool success = checkTerminalsAndIcons(path, variables, cert);

    cert.printSubsectionSummary(success);
}

std::optional<std::string> TerminalsAndIconsCheckerBase::getXmlAttribute(xmlNodePtr node, const std::string& attr_name)
{
    if (!node)
        return std::nullopt;

    xmlChar* attr = xmlGetProp(node, reinterpret_cast<const xmlChar*>(attr_name.c_str()));
    if (!attr)
        return std::nullopt;

    std::string value(reinterpret_cast<char*>(attr));
    xmlFree(attr);
    return value;
}

bool TerminalsAndIconsCheckerBase::checkTerminalsAndIcons(const std::filesystem::path& path,
                                                          const std::map<std::string, TerminalVariableInfo>& variables,
                                                          Certificate& cert)
{
    auto terminals_path = path / "terminalsAndIcons" / "terminalsAndIcons.xml";

    if (!std::filesystem::exists(terminals_path))
    {
        TestResult test{
            "Terminals and Icons File", TestStatus::PASS, {"terminalsAndIcons.xml not present (optional)."}};
        cert.printTestResult(test);
        return true;
    }

    xmlDocPtr doc = xmlReadFile(terminals_path.string().c_str(), nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc)
    {
        TestResult test{"Parse terminalsAndIcons.xml", TestStatus::FAIL, {"Failed to parse terminalsAndIcons.xml."}};
        cert.printTestResult(test);
        return false;
    }

    bool all_passed = true;
    auto print_test = [&](TestResult& test)
    {
        if (test.status == TestStatus::FAIL)
            all_passed = false;
        cert.printTestResult(test);
    };

    xmlNodePtr root = xmlDocGetRootElement(doc);
    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    std::string p = "";
    if (root->ns && root->ns->href)
    {
        xmlXPathRegisterNs(context, reinterpret_cast<const xmlChar*>("f"), root->ns->href);
        p = "f:";
    }

    // 1. Check fmiVersion consistency
    {
        TestResult test{"FMI Version Consistency", TestStatus::PASS, {}};
        checkFmiVersion(root, test);
        print_test(test);
    }

    // 2. Check uniqueness of terminal names on each level
    {
        TestResult test{"Unique Terminal Names", TestStatus::PASS, {}};
        checkUniqueTerminalNames(context, p, test);
        print_test(test);
    }

    // 3. Variable references and constraints
    {
        TestResult test{"Terminal Member Variables", TestStatus::PASS, {}};
        checkVariableReferences(context, p, variables, test);
        print_test(test);
    }

    // 4. Member name uniqueness within a terminal
    {
        TestResult test{"Unique Member Names", TestStatus::PASS, {}};
        checkUniqueMemberNames(context, p, test);
        print_test(test);
    }

    // 5. Stream and inflow/outflow constraint
    {
        TestResult test{"Stream and Flow Constraints", TestStatus::PASS, {}};
        checkStreamFlowConstraints(context, p, test);
        print_test(test);
    }

    // 6. Graphical Representation
    {
        TestResult test{"Graphical Representation", TestStatus::PASS, {}};
        checkGraphicalRepresentation(path, context, p, test);
        print_test(test);
    }

    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);
    return all_passed;
}

void TerminalsAndIconsCheckerBase::checkFmiVersion(xmlNodePtr root, TestResult& test)
{
    auto version_attr = getXmlAttribute(root, "fmiVersion");
    if (!version_attr)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("terminalsAndIcons.xml is missing 'fmiVersion' attribute.");
    }
    else
    {
        // FMI 3.0 specification: fmiVersion must be "3.0"
        if (*version_attr != "3.0")
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("fmiVersion in terminalsAndIcons.xml must be '3.0' (found '" + *version_attr +
                                    "').");
        }
    }
}

void TerminalsAndIconsCheckerBase::checkUniqueTerminalNames(xmlXPathContextPtr context, const std::string& p,
                                                            TestResult& test)
{
    auto check_unique_terminals = [&](auto self, xmlNodePtr parent) -> void
    {
        std::set<std::string> seen_names;
        for (xmlNodePtr child = parent->children; child; child = child->next)
        {
            if (child->type == XML_ELEMENT_NODE &&
                xmlStrcmp(child->name, reinterpret_cast<const xmlChar*>("Terminal")) == 0)
            {
                auto name = getXmlAttribute(child, "name");
                if (name)
                {
                    if (seen_names.contains(*name))
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("Terminal name \"" + *name + "\" (line " + std::to_string(child->line) +
                                                ") is not unique at its level.");
                    }
                    seen_names.insert(*name);
                }
                self(self, child);
            }
        }
    };

    std::string expr = "/" + p + "fmiTerminalsAndIcons/" + p + "Terminals";
    xmlXPathObjectPtr terminals_elem = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.c_str()), context);
    if (terminals_elem && terminals_elem->nodesetval && terminals_elem->nodesetval->nodeNr > 0)
        check_unique_terminals(check_unique_terminals, terminals_elem->nodesetval->nodeTab[0]);
    if (terminals_elem)
        xmlXPathFreeObject(terminals_elem);
}

void TerminalsAndIconsCheckerBase::checkVariableReferences(xmlXPathContextPtr context, const std::string& p,
                                                           const std::map<std::string, TerminalVariableInfo>& variables,
                                                           TestResult& test)
{
    std::string expr = "//" + p + "TerminalMemberVariable";
    xmlXPathObjectPtr member_vars = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.c_str()), context);
    if (member_vars && member_vars->nodesetval)
    {
        for (int i = 0; i < member_vars->nodesetval->nodeNr; ++i)
        {
            xmlNodePtr node = member_vars->nodesetval->nodeTab[i];
            auto var_name = getXmlAttribute(node, "variableName");
            auto var_kind = getXmlAttribute(node, "variableKind");

            if (var_name)
            {
                auto it = variables.find(*var_name);
                if (it == variables.end())
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("TerminalMemberVariable (line " + std::to_string(node->line) +
                                            ") references non-existent variable \"" + *var_name + "\".");
                }
                else
                {
                    const auto& var_info = it->second;
                    if (var_kind)
                    {
                        if (*var_kind == "signal" || *var_kind == "inflow" || *var_kind == "outflow")
                        {
                            if (var_info.causality != "input" && var_info.causality != "output" &&
                                var_info.causality != "parameter" && var_info.causality != "calculatedParameter")
                            {
                                test.status = TestStatus::FAIL;
                                test.messages.push_back("Variable \"" + *var_name + "\" used as '" + *var_kind +
                                                        "' in terminal (line " + std::to_string(node->line) +
                                                        ") must have causality 'input', 'output', 'parameter', or "
                                                        "'calculatedParameter'.");
                            }
                        }
                    }
                }
            }
        }
    }
    if (member_vars)
        xmlXPathFreeObject(member_vars);

    expr = "//" + p + "TerminalStreamMemberVariable";
    xmlXPathObjectPtr stream_vars = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.c_str()), context);
    if (stream_vars && stream_vars->nodesetval)
    {
        for (int i = 0; i < stream_vars->nodesetval->nodeNr; ++i)
        {
            xmlNodePtr node = stream_vars->nodesetval->nodeTab[i];
            auto in_stream = getXmlAttribute(node, "inStreamVariableName");
            auto out_stream = getXmlAttribute(node, "outStreamVariableName");

            if (in_stream)
            {
                auto it = variables.find(*in_stream);
                if (it == variables.end())
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("TerminalStreamMemberVariable (line " + std::to_string(node->line) +
                                            ") references non-existent variable \"" + *in_stream +
                                            "\" in inStreamVariableName.");
                }
                else if (it->second.causality != "input" && it->second.causality != "parameter")
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Variable \"" + *in_stream + "\" used in inStreamVariableName (line " +
                                            std::to_string(node->line) +
                                            ") must have causality 'input' or 'parameter'.");
                }
            }

            if (in_stream && out_stream)
            {
                auto it_in = variables.find(*in_stream);
                auto it_out = variables.find(*out_stream);

                if (it_in != variables.end() && it_out != variables.end())
                {
                    if (it_in->second.dimensions != it_out->second.dimensions)
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("TerminalStreamMemberVariable (line " + std::to_string(node->line) +
                                                ") has mismatched dimensions for \"" + *in_stream + "\" and \"" +
                                                *out_stream + "\".");
                    }
                }
            }

            if (out_stream)
            {
                auto it = variables.find(*out_stream);
                if (it == variables.end())
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("TerminalStreamMemberVariable (line " + std::to_string(node->line) +
                                            ") references non-existent variable \"" + *out_stream +
                                            "\" in outStreamVariableName.");
                }
                else if (it->second.causality != "output" && it->second.causality != "calculatedParameter")
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Variable \"" + *out_stream + "\" used in outStreamVariableName (line " +
                                            std::to_string(node->line) +
                                            ") must have causality 'output' or 'calculatedParameter'.");
                }
            }
        }
    }
    if (stream_vars)
        xmlXPathFreeObject(stream_vars);
}

void TerminalsAndIconsCheckerBase::checkUniqueMemberNames(xmlXPathContextPtr context, const std::string& p,
                                                          TestResult& test)
{
    auto check_unique_members = [&](auto self, xmlNodePtr terminal) -> void
    {
        std::set<std::string> seen_members;
        auto matching_rule = getXmlAttribute(terminal, "matchingRule").value_or("plug");

        for (xmlNodePtr child = terminal->children; child; child = child->next)
        {
            if (child->type == XML_ELEMENT_NODE)
            {
                std::string elem_name = reinterpret_cast<const char*>(child->name);
                if (elem_name == "TerminalMemberVariable")
                {
                    auto member_name = getXmlAttribute(child, "memberName");
                    if (member_name)
                    {
                        if (seen_members.contains(*member_name))
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Member name \"" + *member_name +
                                                    "\" is not unique in terminal (line " +
                                                    std::to_string(child->line) + ").");
                        }
                        seen_members.insert(*member_name);
                    }
                    else if (matching_rule == "plug" || matching_rule == "bus")
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("TerminalMemberVariable (line " + std::to_string(child->line) +
                                                ") is missing mandatory 'memberName' for matchingRule '" +
                                                matching_rule + "'.");
                    }
                }
                else if (elem_name == "TerminalStreamMemberVariable")
                {
                    auto in_member = getXmlAttribute(child, "inStreamMemberName");
                    auto out_member = getXmlAttribute(child, "outStreamMemberName");
                    if (in_member)
                    {
                        if (seen_members.contains(*in_member))
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Stream member name \"" + *in_member +
                                                    "\" is not unique in terminal (line " +
                                                    std::to_string(child->line) + ").");
                        }
                        seen_members.insert(*in_member);
                    }
                    if (out_member && (!in_member || *out_member != *in_member))
                    {
                        if (seen_members.contains(*out_member))
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Stream member name \"" + *out_member +
                                                    "\" is not unique in terminal (line " +
                                                    std::to_string(child->line) + ").");
                        }
                        seen_members.insert(*out_member);
                    }
                }
                else if (elem_name == "Terminal")
                {
                    self(self, child);
                }
            }
        }
    };

    std::string expr = "/" + p + "fmiTerminalsAndIcons/" + p + "Terminals//" + p + "Terminal";
    xmlXPathObjectPtr terminals_elem = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.c_str()), context);
    if (terminals_elem && terminals_elem->nodesetval)
        for (int i = 0; i < terminals_elem->nodesetval->nodeNr; ++i)
            check_unique_members(check_unique_members, terminals_elem->nodesetval->nodeTab[i]);
    if (terminals_elem)
        xmlXPathFreeObject(terminals_elem);
}

void TerminalsAndIconsCheckerBase::checkGraphicalRepresentation(const std::filesystem::path& path,
                                                                xmlXPathContextPtr context, const std::string& p,
                                                                TestResult& test)
{
    std::string expr = "//" + p + "TerminalGraphicalRepresentation";
    xmlXPathObjectPtr xpath_obj = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.c_str()), context);

    if (xpath_obj && xpath_obj->nodesetval)
    {
        for (int i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            auto icon_base = getXmlAttribute(node, "iconBaseName");

            if (icon_base)
            {
                // RFC 3986 relative URI constraints for iconBaseName:
                // excluding relative URIs that move beyond the baseURI (i.e. go "up" a level via ..)
                // no absolute URIs (start with /), no schemes (contains :)
                if (icon_base->find("..") != std::string::npos || icon_base->starts_with("/") ||
                    icon_base->find(":") != std::string::npos)
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("iconBaseName \"" + *icon_base + "\" (line " + std::to_string(node->line) +
                                            ") must be a relative URI and must not contain \"..\", \":\" or start with "
                                            "\"/\".");
                }
                else
                {
                    // PNG file must be provided
                    auto png_path = path / "terminalsAndIcons" / (*icon_base + ".png");
                    if (!std::filesystem::exists(png_path))
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("Terminal icon PNG file \"" + png_path.string() +
                                                "\" (referenced line " + std::to_string(node->line) + ") not found.");
                    }
                }
            }
            else
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("TerminalGraphicalRepresentation (line " + std::to_string(node->line) +
                                        ") is missing mandatory 'iconBaseName'.");
            }

            auto connection_color = getXmlAttribute(node, "defaultConnectionColor");
            if (connection_color)
            {
                // defaultConnectionColor should be RGB values from 0 to 255.
                // It is a list of 3 unsigned bytes.
                std::stringstream ss(*connection_color);
                std::string part;
                int count = 0;
                while (ss >> part)
                    count++;
                if (count != 3)
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("defaultConnectionColor \"" + *connection_color + "\" (line " +
                                            std::to_string(node->line) + ") must have exactly 3 RGB values.");
                }
            }
        }
    }

    if (xpath_obj)
        xmlXPathFreeObject(xpath_obj);
}

void TerminalsAndIconsCheckerBase::checkStreamFlowConstraints(xmlXPathContextPtr context, const std::string& p,
                                                              TestResult& test)
{
    auto check_stream_flow_constraint = [&](auto self, xmlNodePtr terminal) -> void
    {
        bool has_stream = false;
        int flow_count = 0;

        for (xmlNodePtr child = terminal->children; child; child = child->next)
        {
            if (child->type == XML_ELEMENT_NODE)
            {
                std::string elem_name = reinterpret_cast<const char*>(child->name);
                if (elem_name == "TerminalStreamMemberVariable")
                {
                    has_stream = true;
                }
                else if (elem_name == "TerminalMemberVariable")
                {
                    auto var_kind = getXmlAttribute(child, "variableKind");
                    if (var_kind && (*var_kind == "inflow" || *var_kind == "outflow"))
                        flow_count++;
                }
                else if (elem_name == "Terminal")
                {
                    self(self, child);
                }
            }
        }

        if (has_stream && flow_count > 1)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Terminal \"" + getXmlAttribute(terminal, "name").value_or("unnamed") +
                                    "\" (line " + std::to_string(terminal->line) +
                                    ") has multiple inflow/outflow variables and a stream variable. Only one "
                                    "inflow/outflow variable is allowed when a stream variable is present.");
        }
    };

    std::string expr = "/" + p + "fmiTerminalsAndIcons/" + p + "Terminals//" + p + "Terminal";
    xmlXPathObjectPtr terminals_elem = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.c_str()), context);
    if (terminals_elem && terminals_elem->nodesetval)
        for (int i = 0; i < terminals_elem->nodesetval->nodeNr; ++i)
            check_stream_flow_constraint(check_stream_flow_constraint, terminals_elem->nodesetval->nodeTab[i]);
    if (terminals_elem)
        xmlXPathFreeObject(terminals_elem);
}
