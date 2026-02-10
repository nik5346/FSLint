#include "fmi2_terminals_and_icons_checker.h"
#include "certificate.h"
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <regex>

void Fmi2TerminalsAndIconsChecker::checkFmiVersion(xmlNodePtr root, TestResult& test)
{
    auto version_attr = getXmlAttribute(root, "fmiVersion");
    if (!version_attr)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("terminalsAndIcons.xml is missing 'fmiVersion' attribute.");
    }
    else
    {
        // terminalsAndIcons.xml always follows FMI 3.0 versioning rules
        std::regex fmi3_pattern(R"(^3\.(0|[1-9][0-9]*)(\.(0|[1-9][0-9]*))?(-.+)?$)");
        if (!std::regex_match(*version_attr, fmi3_pattern))
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("fmiVersion in terminalsAndIcons.xml must match FMI 3.0 format (found '" +
                                    *version_attr + "').");
        }
    }
}

std::map<std::string, TerminalVariableInfo>
Fmi2TerminalsAndIconsChecker::extractVariables(const std::filesystem::path& path, Certificate& cert,
                                               std::string& fmiVersion)
{
    std::map<std::string, TerminalVariableInfo> variables;
    auto model_desc_path = path / "modelDescription.xml";

    if (!std::filesystem::exists(model_desc_path))
    {
        cert.printTestResult({"Model Description File", TestStatus::FAIL, {"modelDescription.xml not found."}});
        return variables;
    }

    xmlDocPtr doc = xmlReadFile(model_desc_path.string().c_str(), nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc)
    {
        cert.printTestResult({"Parse Model Description", TestStatus::FAIL, {"Failed to parse modelDescription.xml."}});
        return variables;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    auto version_attr = getXmlAttribute(root, "fmiVersion");
    if (version_attr)
    {
        fmiVersion = *version_attr;
    }
    else
    {
        cert.printTestResult(
            {"FMI Version", TestStatus::FAIL, {"modelDescription.xml is missing 'fmiVersion' attribute."}});
        xmlFreeDoc(doc);
        return variables;
    }

    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr xpath_obj =
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//ModelVariables/ScalarVariable"), context);

    if (xpath_obj && xpath_obj->nodesetval)
    {
        for (int i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            TerminalVariableInfo var;
            var.name = getXmlAttribute(node, "name").value_or("");
            var.causality = getXmlAttribute(node, "causality").value_or("local");
            var.variability = getXmlAttribute(node, "variability").value_or("");
            var.sourceline = node->line;

            for (xmlNodePtr child = node->children; child; child = child->next)
            {
                if (child->type == XML_ELEMENT_NODE)
                {
                    std::string elem_name = reinterpret_cast<const char*>(child->name);
                    if (elem_name == "Real" || elem_name == "Integer" || elem_name == "Boolean" ||
                        elem_name == "String" || elem_name == "Enumeration")
                    {
                        var.type = elem_name;
                        break;
                    }
                }
            }

            if (!var.name.empty())
                variables[var.name] = var;
        }
    }

    if (xpath_obj)
        xmlXPathFreeObject(xpath_obj);
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);

    return variables;
}
