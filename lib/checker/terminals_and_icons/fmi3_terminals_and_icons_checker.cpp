#include "fmi3_terminals_and_icons_checker.h"

#include "terminals_and_icons_checker.h"

#include "certificate.h"

#include <libxml/parser.h>
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

void Fmi3TerminalsAndIconsChecker::checkFmiVersion(xmlNodePtr root, TestResult& test)
{
    auto version_attr = getXmlAttribute(root, "fmiVersion");
    if (!version_attr)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("terminalsAndIcons.xml is missing 'fmiVersion' attribute.");
    }
}

std::map<std::string, TerminalVariableInfo>
Fmi3TerminalsAndIconsChecker::extractVariables(const std::filesystem::path& path, Certificate& cert,
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
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//ModelVariables/*"), context);

    if (xpath_obj && xpath_obj->nodesetval)
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
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            var.type = reinterpret_cast<const char*>(node->name);

            for (xmlNodePtr child = node->children; child; child = child->next)
            {
                if (child->type == XML_ELEMENT_NODE &&
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                    xmlStrcmp(child->name, reinterpret_cast<const xmlChar*>("Dimension")) == 0)
                {
                    TerminalDimension dim;
                    auto start_str = getXmlAttribute(child, "start");
                    if (start_str)
                    {
                        try
                        {
                            dim.start = std::stoull(*start_str);
                        }
                        catch (...)
                        {
                        }
                    }
                    auto vr_str = getXmlAttribute(child, "valueReference");
                    if (vr_str)
                    {
                        try
                        {
                            dim.value_reference = static_cast<uint32_t>(std::stoul(*vr_str));
                        }
                        catch (...)
                        {
                        }
                    }
                    var.dimensions.push_back(dim);
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
