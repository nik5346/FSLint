#include "directory_checker.h"

#include "certificate.h"
#include "file_utils.h"
#include "xml_utils.h"

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlstring.h>

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

void DirectoryChecker::validate(const std::filesystem::path& path, Certificate& cert) const
{
    cert.printSubsectionHeader("FMU DIRECTORY STRUCTURE");

    // Mandatory modelDescription.xml check
    const std::filesystem::path model_desc_path = path / "modelDescription.xml";
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

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (root)
    {
        // FMI 1.0 extraction
        const std::optional<std::string> model_id_attr = getXmlAttribute(root, "modelIdentifier");
        if (model_id_attr.has_value())
        {
            // By default ME in FMI 1.0
            const auto& val = *model_id_attr;
            model_identifiers["ModelExchange"] = val;
        }

        for (xmlNodePtr node = root->children; node; node = node->next)
        {
            if (node->type != XML_ELEMENT_NODE)
                continue;

            const xmlChar* name = node->name;
            if (xmlStrEqual(name, reinterpret_cast<const xmlChar*>("CoSimulation")) ||
                xmlStrEqual(name, reinterpret_cast<const xmlChar*>("ModelExchange")) ||
                xmlStrEqual(name, reinterpret_cast<const xmlChar*>("ScheduledExecution")))
            {
                const std::optional<std::string> model_id = getXmlAttribute(node, "modelIdentifier");
                if (model_id.has_value())
                {
                    const auto& val = *model_id;
                    model_identifiers[reinterpret_cast<const char*>(name)] = val;

                    // If we found CoSimulation in FMI 1.0, move the model identifier there
                    if (xmlStrEqual(name, reinterpret_cast<const xmlChar*>("CoSimulation")) &&
                        model_id_attr.has_value())
                    {
                        const auto& attr_val = *model_id_attr;
                        model_identifiers.erase("ModelExchange");
                        model_identifiers["CoSimulation"] = attr_val;
                    }
                }

                if (!xmlStrEqual(name, reinterpret_cast<const xmlChar*>("ScheduledExecution")))
                {
                    const std::optional<std::string> needs_exec = getXmlAttribute(node, "needsExecutionTool");
                    if (needs_exec.has_value() && *needs_exec == "true")
                        needs_execution_tool = true;
                }

                // Check for SourceFiles inside interface (FMI 2.0)
                for (xmlNodePtr child = node->children; child; child = child->next)
                {
                    if (child->type == XML_ELEMENT_NODE &&
                        xmlStrEqual(child->name, reinterpret_cast<const xmlChar*>("SourceFiles")))
                    {
                        for (xmlNodePtr file_node = child->children; file_node; file_node = file_node->next)
                        {
                            if (file_node->type == XML_ELEMENT_NODE &&
                                xmlStrEqual(file_node->name, reinterpret_cast<const xmlChar*>("File")))
                            {
                                const std::optional<std::string> name_opt = getXmlAttribute(file_node, "name");
                                if (name_opt.has_value())
                                {
                                    const auto& val = *name_opt;
                                    listed_sources_in_md.insert(val);
                                }
                            }
                        }
                    }
                }
            }
            else if (xmlStrEqual(name, reinterpret_cast<const xmlChar*>("Implementation")))
            {
                // In FMI 1.0 CS, Implementation exists but modelIdentifier is still on the root
                if (model_id_attr.has_value())
                {
                    const auto& attr_val = *model_id_attr;
                    model_identifiers.erase("ModelExchange");
                    model_identifiers["CoSimulation"] = attr_val;
                }
            }
        }
    }
    xmlFreeDoc(doc);

    performVersionSpecificChecks(path, cert, model_identifiers, listed_sources_in_md, needs_execution_tool);
    if (cert.shouldAbort())
        return;

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
                                            const std::set<std::string>& headers) const
{
    auto sources_path = path / "sources";
    if (!std::filesystem::exists(sources_path) || !std::filesystem::is_directory(sources_path))
        return;

    TestResult test{"Standard Headers", TestStatus::PASS, {}};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(sources_path))
    {
        if (entry.is_regular_file())
        {
            const std::string filename = file_utils::pathToUtf8(entry.path().filename());
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
        const std::string filename = file_utils::pathToUtf8(entry.path().filename());
        if (filename != ".gitkeep" && filename != ".DS_Store" && filename != "Thumbs.db")
            return false;
    }
    return true;
}
