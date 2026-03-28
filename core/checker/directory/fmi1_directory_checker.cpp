#include "fmi1_directory_checker.h"

#include "certificate.h"
#include "file_utils.h"
#include "xml_utils.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>

#include <filesystem>
#include <format>
#include <map>
#include <set>
#include <string>

void Fmi1DirectoryChecker::validate(const std::filesystem::path& path, Certificate& cert) const
{
    cert.printSubsectionHeader("DIRECTORY STRUCTURE");

    const auto& original_path = getOriginalPath();
    const std::string stem =
        original_path.empty() ? file_utils::pathToUtf8(path.stem()) : file_utils::pathToUtf8(original_path.stem());

    auto model_desc_path = path / "modelDescription.xml";
    if (!std::filesystem::exists(model_desc_path))
    {
        const TestResult test{"Mandatory Files", TestStatus::FAIL, {"modelDescription.xml not found."}};
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    xmlDocPtr doc = readXmlFile(model_desc_path);
    if (!doc)
    {
        const TestResult test{
            "Parse modelDescription.xml", TestStatus::FAIL, {"Failed to parse 'modelDescription.xml'."}};
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    std::map<std::string, std::string> model_identifiers;
    xmlNodePtr root = xmlDocGetRootElement(doc);
    auto model_id = getXmlAttribute(root, "modelIdentifier");
    if (model_id)
    {
        bool is_cs = false;
        xmlXPathContextPtr xpath_context = xmlXPathNewContext(doc);
        if (xpath_context)
        {
            xmlXPathObjectPtr xpath_obj =
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
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

        // FMI 1.0 rule: modelIdentifier must match filename stem
        if (*model_id != stem)
        {
            TestResult test{"Model Identifier Filename Match", TestStatus::FAIL, {}};
            test.messages.push_back(
                std::format("modelIdentifier '{}' must match the FMU filename '{}'.", *model_id, stem));
            cert.printTestResult(test);
        }
        else
        {
            cert.printTestResult({"Model Identifier Filename Match", TestStatus::PASS, {}});
        }
    }
    xmlFreeDoc(doc);

    performVersionSpecificChecks(path, cert, model_identifiers, {}, false);
    cert.printSubsectionSummary(true);
}

void Fmi1DirectoryChecker::performVersionSpecificChecks(
    const std::filesystem::path& path, Certificate& cert, const std::map<std::string, std::string>& model_identifiers,
    [[maybe_unused]] const std::set<std::string>& listed_sources_in_md,
    [[maybe_unused]] bool needs_execution_tool) const
{
    // 1. FMU Root Entries
    {
        TestResult test{"FMU Root Entries", TestStatus::PASS, {}};
        static const std::set<std::string> fmi1_standard_entries = {
            "modelDescription.xml", "model.png", "documentation", "sources", "binaries", "resources"};

        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            const std::string name = file_utils::pathToUtf8(entry.path().filename());
            // Ignore .gitkeep
            if (name == ".gitkeep")
                continue;

            if (!fmi1_standard_entries.contains(name))
            {
                test.status = TestStatus::WARNING;
                const std::string type = entry.is_directory() ? "directory" : "file";
                test.messages.push_back(std::format("Unknown {} in FMU root: '{}'.", type, name));
            }

            if (entry.is_directory() && fmi1_standard_entries.contains(name) && isEffectivelyEmpty(entry.path()))
            {
                test.status = TestStatus::WARNING;
                test.messages.push_back("Standard directory '" + name + "' is empty.");
            }
        }
        cert.printTestResult(test);
    }

    // 2. model.png Existence
    {
        TestResult test{"model.png Existence", TestStatus::PASS, {}};
        auto png_path = path / "model.png";
        if (!std::filesystem::exists(png_path))
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Recommended file 'model.png' is missing from the FMU root.");
        }
        else
        {
            auto dimensions = file_utils::getPngDimensions(png_path);
            if (dimensions)
            {
                if (dimensions->first < 100 || dimensions->second < 100)
                {
                    test.status = TestStatus::WARNING;
                    test.messages.push_back(
                        std::format("Icon 'model.png' is small ({}x{} pixels). A size of at least 100x100 pixels is "
                                    "recommended.",
                                    dimensions->first, dimensions->second));
                }
            }
        }
        cert.printTestResult(test);
    }

    // 3. Documentation
    {
        TestResult test{"Documentation", TestStatus::PASS, {}};
        auto doc_path = path / "documentation";
        if (std::filesystem::exists(doc_path))
        {
            if (!std::filesystem::exists(doc_path / "_main.html"))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("The documentation entry point 'documentation/_main.html' is missing.");
            }
        }
        else
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Providing documentation is recommended.");
        }
        cert.printTestResult(test);
    }

    // 4. Distribution (Binaries and Sources)
    {
        TestResult test{"Binaries and Sources", TestStatus::PASS, {}};
        bool has_binaries = false;
        if (std::filesystem::exists(path / "binaries"))
        {
            std::set<std::string> unique_model_ids;
            for (const auto& [interface, model_id] : model_identifiers)
                unique_model_ids.insert(model_id);

            for (const auto& entry : std::filesystem::directory_iterator(path / "binaries"))
            {
                if (entry.is_directory())
                {
                    const std::string platform = file_utils::pathToUtf8(entry.path().filename());
                    for (const auto& model_id : unique_model_ids)
                    {
                        bool found_model_id = false;
                        for (const auto& ext : {".dll", ".so", ".dylib"})
                        {
                            if (std::filesystem::exists(entry.path() / (model_id + ext)))
                            {
                                found_model_id = true;
                                has_binaries = true;
                                break;
                            }
                        }

                        if (!found_model_id)
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back(
                                std::format("Platform directory '{}' does not contain a binary matching "
                                            "modelIdentifier '{}'.",
                                            platform, model_id));
                        }
                    }
                }
            }
        }

        auto sources_path = path / "sources";
        const bool has_sources = std::filesystem::exists(sources_path) && !std::filesystem::is_empty(sources_path);

        if (!has_binaries && !has_sources)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back(
                "FMU must contain either a precompiled binary for at least one platform or source code.");
        }
        cert.printTestResult(test);
    }

    // 5. Standard Headers
    static const std::set<std::string> fmi1_headers = {"fmiFunctions.h", "fmiModelFunctions.h", "fmiModelTypes.h",
                                                       "fmiPlatformTypes.h"};
    checkStandardHeaders(path, cert, fmi1_headers);
}
