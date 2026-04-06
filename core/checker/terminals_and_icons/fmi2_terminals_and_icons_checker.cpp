#include "fmi2_terminals_and_icons_checker.h"

#include "terminals_and_icons_checker.h"

#include "certificate.h"
#include "xml_utils.h"

#include <libxml/parser.h>
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>

#include <filesystem>
#include <map>
#include <string>

void Fmi2TerminalsAndIconsChecker::checkFmiVersion(xmlNodePtr root, TestResult& test) const
{
    const auto version_attr = getXmlAttribute(root, "fmiVersion");
    if (!version_attr)
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back("terminalsAndIcons.xml is missing 'fmiVersion' attribute.");
    }
}

std::map<std::string, TerminalVariableInfo>
Fmi2TerminalsAndIconsChecker::extractVariables(const std::filesystem::path& path, Certificate& cert,
                                               std::string& fmiVersion) const
{
    std::map<std::string, TerminalVariableInfo> variables;
    auto model_desc_path = path / "modelDescription.xml";

    if (!std::filesystem::exists(model_desc_path))
    {
        cert.printTestResult({"Model Description File", TestStatus::FAIL, {"modelDescription.xml not found."}});
        return variables;
    }

    xmlDocPtr doc = readXmlFile(model_desc_path);
    if (doc == nullptr)
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
            {"Version", TestStatus::FAIL, {"modelDescription.xml is missing 'fmiVersion' attribute."}});
        xmlFreeDoc(doc);
        return variables;
    }

    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr xpath_obj =
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//ModelVariables/ScalarVariable"), context);

    if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
    {
        for (int i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
            TerminalVariableInfo var;
            var.name = getXmlAttribute(node, "name").value_or("");
            var.causality = getXmlAttribute(node, "causality").value_or("local");
            var.variability = getXmlAttribute(node, "variability").value_or("");
            var.sourceline = node->line;

            for (xmlNodePtr child = node->children; child != nullptr; child = child->next)
            {
                if (child->type == XML_ELEMENT_NODE)
                {
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                    const std::string elem_name = reinterpret_cast<const char*>(child->name);
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

    if (xpath_obj != nullptr)
        xmlXPathFreeObject(xpath_obj);
    xmlXPathFreeContext(context);
    xmlFreeDoc(doc);

    return variables;
}
