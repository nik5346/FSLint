#include "directory_checker.h"
#include "certificate.h"
#include <algorithm>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <set>

void DirectoryChecker::validate(const std::filesystem::path& path, Certificate& cert)
{
    cert.printSubsectionHeader("FMU DIRECTORY STRUCTURE");

    TestResult test{"FMU Structure", TestStatus::PASS, {}};

    // 1. modelDescription.xml (mandatory)
    auto model_desc_path = path / "modelDescription.xml";
    if (!std::filesystem::exists(model_desc_path))
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Mandatory file 'modelDescription.xml' is missing from the FMU root.");
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    // Parse modelDescription.xml to get modelIdentifiers and fmiVersion
    xmlDocPtr doc = xmlReadFile(model_desc_path.string().c_str(), nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Failed to parse 'modelDescription.xml' to verify directory structure.");
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    auto fmi_version_opt = getXmlAttribute(root, "fmiVersion");
    if (!fmi_version_opt)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Missing 'fmiVersion' in 'modelDescription.xml'.");
        xmlFreeDoc(doc);
        cert.printTestResult(test);
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

        // Check for SourceFiles in MD
        bool has_sources_in_md = false;
        xmlXPathObjectPtr sources_xpath =
            xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//SourceFiles/File"), xpath_context);
        if (sources_xpath && sources_xpath->nodesetval && sources_xpath->nodesetval->nodeNr > 0)
            has_sources_in_md = true;
        if (sources_xpath)
            xmlXPathFreeObject(sources_xpath);

        xmlXPathFreeContext(xpath_context);

        // Check for binaries
        bool has_binaries = false;
        if (std::filesystem::exists(path / "binaries"))
        {
            for (const auto& entry : std::filesystem::directory_iterator(path / "binaries"))
            {
                if (entry.is_directory())
                {
                    for (const auto& [interface, model_id] : model_identifiers)
                    {
                        for (const auto& ext : {".dll", ".so", ".dylib"})
                        {
                            if (std::filesystem::exists(entry.path() / (model_id + ext)))
                            {
                                has_binaries = true;
                                break;
                            }
                        }
                        if (has_binaries)
                            break;
                    }
                }
                if (has_binaries)
                    break;
            }
        }

        bool has_build_description = std::filesystem::exists(path / "sources" / "buildDescription.xml");
        bool has_sources = has_sources_in_md || has_build_description;

        if (!has_binaries && !has_sources)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back(
                "FMU must contain either a precompiled binary for at least one platform or source code.");
        }
    }

    // Check for standard directories/files and warn about unknown ones
    std::set<std::string> standard_entries = {"modelDescription.xml",
                                              "model.png",
                                              "documentation",
                                              "licenses",
                                              "sources",
                                              "binaries",
                                              "resources",
                                              "extra",
                                              "terminalsAndIcons",
                                              "model.svg",
                                              "fmi3Terminals.xml"};

    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        std::string name = entry.path().filename().string();
        if (!standard_entries.contains(name))
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("Unknown entry in FMU root: '" + name + "'.");
        }
    }

    xmlFreeDoc(doc);
    cert.printTestResult(test);
    cert.printSubsectionSummary(test.status != TestStatus::FAIL);
}

std::optional<std::string> DirectoryChecker::getXmlAttribute(xmlNodePtr node, const std::string& attr_name)
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
