#include "directory_checker.h"
#include "certificate.h"
#include <algorithm>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <set>

void DirectoryChecker::validate(const std::filesystem::path& path, Certificate& cert)
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

    if (_fmi_version.starts_with("2."))
    {
        validateFmi2Structure(path, cert, model_identifiers, listed_sources_in_md);
    }
    else if (_fmi_version.starts_with("3."))
    {
        validateFmi3Structure(path, cert, model_identifiers);
    }
    else
    {
        TestResult test{"FMU Structure", TestStatus::WARNING, {"Unknown FMI version " + _fmi_version + "."}};
        cert.printTestResult(test);
        cert.printSubsectionSummary(true);
    }
}

void DirectoryChecker::validateFmi2Structure(const std::filesystem::path& path, Certificate& cert,
                                             const std::map<std::string, std::string>& model_identifiers,
                                             const std::set<std::string>& listed_sources_in_md)
{
    TestResult test{"FMU Structure", TestStatus::PASS, {}};

    static const std::set<std::string> fmi2_standard_entries = {"modelDescription.xml", "model.png", "documentation",
                                                                "licenses",             "sources",   "binaries",
                                                                "resources",            "extra",     "terminalsAndIcons",
                                                                "buildDescription.xml"};

    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        std::string name = entry.path().filename().string();
        if (!fmi2_standard_entries.contains(name))
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("Unknown entry in FMU root: '" + name + "'.");
        }
    }

    // Check for model.png in root
    if (!std::filesystem::exists(path / "model.png"))
    {
        if (test.status == TestStatus::PASS)
            test.status = TestStatus::WARNING;
        test.messages.push_back("FMI 2.0: Recommended file 'model.png' is missing from the FMU root.");
    }

    // Distribution check
    bool has_binaries = false;
    if (std::filesystem::exists(path / "binaries"))
    {
        for (const auto& entry : std::filesystem::directory_iterator(path / "binaries"))
        {
            if (entry.is_directory())
            {
                for (const auto& [interface, model_id] : model_identifiers)
                {
                    for (const auto& ext : {".dll", ".so", ".dylib", ".lib", ".a"})
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
    bool has_build_description =
        std::filesystem::exists(sources_path / "buildDescription.xml") || std::filesystem::exists(path / "buildDescription.xml");
    bool has_sources = !listed_sources_in_md.empty() || has_build_description ||
                       (std::filesystem::exists(sources_path) && !std::filesystem::is_empty(sources_path));

    if (!has_binaries && !has_sources)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("FMU must contain either a precompiled binary for at least one platform or source code.");
    }

    // Reverse check for FMI 2.0 legacy sources (only if no buildDescription.xml)
    if (std::filesystem::exists(sources_path) && !has_build_description)
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(sources_path))
        {
            if (entry.is_regular_file())
            {
                auto rel_path = std::filesystem::relative(entry.path(), sources_path);
                std::string filename = rel_path.string();
                std::replace(filename.begin(), filename.end(), '\\', '/'); // Normalize paths

                // Only check typical source files
                std::string ext = entry.path().extension().string();
                for (auto& c : ext)
                    c = static_cast<char>(std::tolower(c));

                if (ext == ".c" || ext == ".cpp" || ext == ".cxx" || ext == ".cc")
                {
                    if (!listed_sources_in_md.contains(filename))
                    {
                        if (test.status == TestStatus::PASS)
                            test.status = TestStatus::WARNING;
                        test.messages.push_back("Source file '" + filename +
                                                "' exists in 'sources/' directory but is not listed in "
                                                "'modelDescription.xml'.");
                    }
                }
            }
        }
    }

    // FMI 2 compatibility warnings
    if (has_sources)
    {
        bool has_sources_in_md = !listed_sources_in_md.empty();
        if (has_sources_in_md && !has_build_description)
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("FMI 2.0 source code FMU only contains <SourceFiles> in modelDescription.xml. "
                                    "It is recommended to also provide a buildDescription.xml for FMI 2.0.4+ "
                                    "compatibility.");
        }
        else if (!has_sources_in_md && has_build_description)
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("FMI 2.0 source code FMU only contains buildDescription.xml. For backwards "
                                    "compatibility with older FMI 2.0 importers, it is recommended to also provide "
                                    "<SourceFiles> in modelDescription.xml.");
        }
    }

    cert.printTestResult(test);
    cert.printSubsectionSummary(test.status != TestStatus::FAIL);
}

void DirectoryChecker::validateFmi3Structure(const std::filesystem::path& path, Certificate& cert,
                                             const std::map<std::string, std::string>& model_identifiers)
{
    TestResult test{"FMU Structure", TestStatus::PASS, {}};

    static const std::set<std::string> fmi3_standard_entries = {"modelDescription.xml", "documentation",
                                                                "terminalsAndIcons",    "sources",
                                                                "binaries",             "resources",
                                                                "extra"};

    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        std::string name = entry.path().filename().string();
        if (!fmi3_standard_entries.contains(name))
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("Unknown entry in FMU root: '" + name + "'.");
        }
    }

    // FMI 3 documentation/ check
    if (std::filesystem::exists(path / "documentation"))
    {
        if (std::filesystem::exists(path / "documentation" / "diagram.svg") &&
            !std::filesystem::exists(path / "documentation" / "diagram.png"))
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("FMI 3.0: diagram.svg exists in documentation/ but diagram.png is missing (required "
                                    "if diagram.svg is provided).");
        }
    }

    // FMI 3 terminalsAndIcons/ check
    if (std::filesystem::exists(path / "terminalsAndIcons"))
    {
        if (std::filesystem::exists(path / "terminalsAndIcons" / "icon.svg") &&
            !std::filesystem::exists(path / "terminalsAndIcons" / "icon.png"))
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back(
                "FMI 3.0: icon.svg exists in terminalsAndIcons/ but icon.png is missing (required "
                "if icon.svg is provided).");
        }
    }

    // Distribution check
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

    bool has_sources = std::filesystem::exists(path / "sources" / "buildDescription.xml") ||
                       (std::filesystem::exists(path / "sources") && !std::filesystem::is_empty(path / "sources"));

    if (!has_binaries && !has_sources)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("FMU must contain either a precompiled binary for at least one platform or source code.");
    }

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
