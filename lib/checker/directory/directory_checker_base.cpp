#include "directory_checker_base.h"
#include "certificate.h"
#include <filesystem>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

void DirectoryCheckerBase::validate(const std::filesystem::path& path, Certificate& cert)
{
    cert.printSubsectionHeader("FMU DIRECTORY STRUCTURE");

    // Mandatory modelDescription.xml check
    auto model_desc_path = path / "modelDescription.xml";
    if (!std::filesystem::exists(model_desc_path))
    {
        TestResult test{"FMU Structure", TestStatus::FAIL,
                        {"Mandatory file 'modelDescription.xml' is missing from the FMU root."}};
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    // Parse modelDescription.xml to get modelIdentifiers and check for legacy SourceFiles
    xmlDocPtr doc = xmlReadFile(model_desc_path.string().c_str(), nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc)
    {
        TestResult test{"FMU Structure", TestStatus::FAIL,
                        {"Failed to parse 'modelDescription.xml' to verify directory structure."}};
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    std::map<std::string, std::string> model_identifiers;
    std::set<std::string> listed_sources_in_md;

    std::vector<std::string> interface_elements = {"CoSimulation", "ModelExchange", "ScheduledExecution"};
    xmlXPathContextPtr xpath_context = xmlXPathNewContext(doc);
    if (xpath_context)
    {
        for (const auto& elem : interface_elements)
        {
            std::string xpath = "//" + elem;
            xmlXPathObjectPtr xpath_obj =
                xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(xpath.c_str()), xpath_context);
            if (xpath_obj && xpath_obj->nodesetval && xpath_obj->nodesetval->nodeNr > 0)
            {
                auto model_id = getXmlAttribute(xpath_obj->nodesetval->nodeTab[0], "modelIdentifier");
                if (model_id)
                    model_identifiers[elem] = *model_id;
            }
            if (xpath_obj)
                xmlXPathFreeObject(xpath_obj);
        }

        xmlXPathObjectPtr sources_xpath =
            xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//SourceFiles/File"), xpath_context);
        if (sources_xpath && sources_xpath->nodesetval)
        {
            for (int i = 0; i < sources_xpath->nodesetval->nodeNr; ++i)
            {
                auto node = sources_xpath->nodesetval->nodeTab[i];
                auto name_opt = getXmlAttribute(node, "name");
                if (name_opt)
                    listed_sources_in_md.insert(*name_opt);
            }
        }
        if (sources_xpath)
            xmlXPathFreeObject(sources_xpath);

        xmlXPathFreeContext(xpath_context);
    }
    xmlFreeDoc(doc);

    performVersionSpecificChecks(path, cert, model_identifiers, listed_sources_in_md);
}

std::optional<std::string> DirectoryCheckerBase::getXmlAttribute(xmlNodePtr node, const std::string& attr_name)
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
