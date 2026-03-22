#include "directory_checker.h"

#include "certificate.h"
#include "xml_utils.h"

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

void DirectoryChecker::validate(const std::filesystem::path& path, Certificate& cert)
{
    cert.printSubsectionHeader("FMU DIRECTORY STRUCTURE");

    // Mandatory modelDescription.xml check
    auto model_desc_path = path / "modelDescription.xml";
    if (!std::filesystem::exists(model_desc_path))
    {
        const TestResult test{"Mandatory Files",
                              TestStatus::FAIL,
                              {"Mandatory file 'modelDescription.xml' is missing from the FMU root."}};
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    // Parse modelDescription.xml to get modelIdentifiers and check for legacy SourceFiles
    xmlDocPtr doc = readXmlFile(model_desc_path);
    if (!doc)
    {
        const TestResult test{"Parse modelDescription.xml",
                              TestStatus::FAIL,
                              {"Failed to parse 'modelDescription.xml' to verify directory structure."}};
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    std::map<std::string, std::string> model_identifiers;
    std::set<std::string> listed_sources_in_md;
    bool needs_execution_tool = false;

    static const std::vector<std::string> interface_elements = {"CoSimulation", "ModelExchange", "ScheduledExecution"};
    xmlXPathContextPtr xpath_context = xmlXPathNewContext(doc);
    if (xpath_context)
    {
        for (const auto& elem : interface_elements)
        {
            const std::string xpath = "//" + elem;
            xmlXPathObjectPtr xpath_obj =
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(xpath.c_str()), xpath_context);
            if (xpath_obj && xpath_obj->nodesetval && xpath_obj->nodesetval->nodeNr > 0)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                auto node = xpath_obj->nodesetval->nodeTab[0];
                auto model_id = getXmlAttribute(node, "modelIdentifier");
                if (model_id)
                    model_identifiers[elem] = *model_id;

                if (elem != "ScheduledExecution")
                {
                    auto needs_exec = getXmlAttribute(node, "needsExecutionTool");
                    if (needs_exec == "true")
                        needs_execution_tool = true;
                }
            }
            if (xpath_obj)
                xmlXPathFreeObject(xpath_obj);
        }

        xmlXPathObjectPtr sources_xpath = xmlXPathEvalExpression(
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            reinterpret_cast<const xmlChar*>("//*[local-name()='SourceFiles']/*[local-name()='File']"), xpath_context);

        if (sources_xpath && sources_xpath->nodesetval)
        {
            for (int i = 0; i < sources_xpath->nodesetval->nodeNr; ++i)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
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

    performVersionSpecificChecks(path, cert, model_identifiers, listed_sources_in_md, needs_execution_tool);

    cert.printSubsectionSummary(true);
}

std::optional<std::string> DirectoryChecker::getXmlAttribute(xmlNodePtr node, const std::string& attr_name)
{
    if (!node)
        return std::nullopt;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    xmlChar* attr = xmlGetProp(node, reinterpret_cast<const xmlChar*>(attr_name.c_str()));
    if (!attr)
        return std::nullopt;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    std::string value(reinterpret_cast<char*>(attr));
    xmlFree(attr);
    return value;
}

void DirectoryChecker::checkStandardHeaders(const std::filesystem::path& path, Certificate& cert,
                                            const std::set<std::string>& headers)
{
    auto sources_path = path / "sources";
    if (!std::filesystem::exists(sources_path) || !std::filesystem::is_directory(sources_path))
        return;

    TestResult test{"Standard Headers", TestStatus::PASS, {}};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(sources_path))
    {
        if (entry.is_regular_file())
        {
            const std::string filename = entry.path().filename().string();
            if (headers.contains(filename))
            {
                test.status = TestStatus::WARNING;
                test.messages.push_back("Standard FMI header file '" + filename +
                                        "' found in 'sources/' directory. This is not recommended as these headers "
                                        "should be provided by the environment.");
            }
        }
    }
    cert.printTestResult(test);
}

bool DirectoryChecker::isEffectivelyEmpty(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path))
        return true;
    if (!std::filesystem::is_directory(path))
        return false;

    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        const auto filename = entry.path().filename();
        if (filename != ".gitkeep" && filename != ".DS_Store" && filename != "Thumbs.db")
            return false;
    }
    return true;
}
