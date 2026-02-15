#include "binary_checker.h"

#include "binary_parser.h"
#include "certificate.h"

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xmlstring.h>

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <vector>

void BinaryChecker::validate(const std::filesystem::path& path, Certificate& cert)
{
    cert.printSubsectionHeader("FMU BINARY EXPORTS");

    auto model_desc_path = path / "modelDescription.xml";
    if (!std::filesystem::exists(model_desc_path))
    {
        cert.printSubsectionSummary(false);
        return;
    }

    xmlDocPtr doc = xmlReadFile(model_desc_path.string().c_str(), nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc)
    {
        cert.printSubsectionSummary(false);
        return;
    }

    std::map<std::string, std::string> model_identifiers;
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
        xmlXPathFreeContext(xpath_context);
    }
    xmlFreeDoc(doc);

    if (model_identifiers.empty())
    {
        cert.printSubsectionSummary(true);
        return;
    }

    std::vector<std::string> expected_functions = getExpectedFunctions();

    auto binaries_path = path / "binaries";
    if (std::filesystem::exists(binaries_path))
    {
        for (const auto& platform_entry : std::filesystem::directory_iterator(binaries_path))
        {
            if (!platform_entry.is_directory())
                continue;

            std::string platform = platform_entry.path().filename().string();

            for (const auto& [interface, model_id] : model_identifiers)
            {
                std::vector<std::string> extensions = {".dll", ".so", ".dylib"};
                for (const auto& ext : extensions)
                {
                    auto binary_file = platform_entry.path() / (model_id + ext);
                    if (std::filesystem::exists(binary_file))
                    {
                        TestResult test{"Exported Functions: " + platform + "/" + model_id + ext, TestStatus::PASS, {}};
                        std::set<std::string> actual_exports = BinaryParser::getExports(binary_file);

                        for (const auto& func : expected_functions)
                        {
                            if (!actual_exports.contains(func))
                            {
                                test.status = TestStatus::FAIL;
                                test.messages.push_back("Mandatory function '" + func + "' is not exported.");
                            }
                        }

                        cert.printTestResult(test);
                    }
                }
            }
        }
    }

    cert.printSubsectionSummary(true);
}

std::optional<std::string> BinaryChecker::getXmlAttribute(xmlNodePtr node, const std::string& attr_name)
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
