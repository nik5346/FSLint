#include "fmi1_directory_checker.h"
#include "certificate.h"
#include <algorithm>
#include <libxml/xpath.h>
#include <set>

void Fmi1DirectoryChecker::validate(const std::filesystem::path& path, Certificate& cert)
{
    cert.printSubsectionHeader("FMI 1.0 DIRECTORY STRUCTURE");

    auto model_desc_path = path / "modelDescription.xml";
    if (!std::filesystem::exists(model_desc_path))
    {
        TestResult test{"Mandatory Files", TestStatus::FAIL, {"modelDescription.xml not found."}};
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    xmlDocPtr doc = xmlReadFile(model_desc_path.string().c_str(), nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc)
    {
        TestResult test{"Parse modelDescription.xml", TestStatus::FAIL, {"Failed to parse 'modelDescription.xml'."}};
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    std::map<std::string, std::string> model_identifiers;
    xmlNodePtr root = xmlDocGetRootElement(doc);
    auto model_id = getXmlAttribute(root, "modelIdentifier");
    if (model_id)
    {
        // Check if modelIdentifier matches filename (FMI 1.0 specific)
        const auto& path_to_check = m_original_path.empty() ? path : m_original_path;
        std::string expected_id = path_to_check.stem().string();

        if (*model_id != expected_id)
        {
            TestResult test{"Model Identifier Filename Match", TestStatus::FAIL,
                            {"FMI 1.0: modelIdentifier '" + *model_id + "' must match the FMU filename '" +
                             expected_id + "'."}};
            cert.printTestResult(test);
        }
        else
        {
            cert.printTestResult({"Model Identifier Filename Match", TestStatus::PASS, {}});
        }

        bool is_cs = false;
        xmlXPathContextPtr xpath_context = xmlXPathNewContext(doc);
        if (xpath_context)
        {
            xmlXPathObjectPtr xpath_obj =
                xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//Implementation"), xpath_context);
            if (xpath_obj && xpath_obj->nodesetval && xpath_obj->nodesetval->nodeNr > 0)
                is_cs = true;
            if (xpath_obj)
                xmlXPathFreeObject(xpath_obj);
            xmlXPathFreeContext(xpath_context);
        }

        if (is_cs)
            model_identifiers["CoSimulation"] = *model_id;
        else
            model_identifiers["ModelExchange"] = *model_id;
    }
    xmlFreeDoc(doc);

    performVersionSpecificChecks(path, cert, model_identifiers, {}, false);
    cert.printSubsectionSummary(true);
}

void Fmi1DirectoryChecker::performVersionSpecificChecks(
    const std::filesystem::path& path, Certificate& cert, const std::map<std::string, std::string>& model_identifiers,
    [[maybe_unused]] const std::set<std::string>& listed_sources_in_md, [[maybe_unused]] bool needs_execution_tool)
{
    // 1. FMU Root Entries
    {
        TestResult test{"FMU Root Entries (FMI1)", TestStatus::PASS, {}};
        static const std::set<std::string> fmi1_standard_entries = {
            "modelDescription.xml", "model.png", "documentation", "sources", "binaries", "resources"};

        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            std::string name = entry.path().filename().string();
            // Ignore .gitkeep
            if (name == ".gitkeep")
                continue;

            if (!fmi1_standard_entries.contains(name))
            {
                test.status = TestStatus::WARNING;
                std::string type = entry.is_directory() ? "directory" : "file";
                test.messages.push_back("Unknown " + type + " in FMU root: '" + name + "'.");
            }
        }
        cert.printTestResult(test);
    }

    // 2. Documentation entry point
    {
        auto doc_path = path / "documentation";
        if (std::filesystem::exists(doc_path) && !std::filesystem::is_empty(doc_path))
        {
            TestResult test{"Documentation Entry Point", TestStatus::PASS, {}};
            if (!std::filesystem::exists(doc_path / "_main.html"))
            {
                test.status = TestStatus::WARNING;
                test.messages.push_back("FMI 1.0: Recommended entry point 'documentation/_main.html' is missing.");
            }
            cert.printTestResult(test);
        }
    }

    // 3. Distribution (Binaries and Sources)
    {
        TestResult test{"Binaries and Sources", TestStatus::PASS, {}};
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

        auto sources_path = path / "sources";
        bool has_sources = std::filesystem::exists(sources_path) && !std::filesystem::is_empty(sources_path);

        if (!has_binaries && !has_sources)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back(
                "FMU must contain either a precompiled binary for at least one platform or source code.");
        }
        cert.printTestResult(test);
    }

    // 4. Standard Headers
    static const std::set<std::string> fmi1_headers = {
        "fmiFunctions.h", "fmiModelFunctions.h", "fmiModelTypes.h", "fmiPlatformTypes.h"};
    checkStandardHeaders(path, cert, fmi1_headers);
}
