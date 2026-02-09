#include "build_description_checker.h"
#include "certificate.h"
#include <algorithm>
#include <filesystem>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <set>

void BuildDescriptionChecker::validate(const std::filesystem::path& path, Certificate& cert)
{
    auto build_desc_path = path / "sources" / "buildDescription.xml";
    if (!std::filesystem::exists(build_desc_path))
        return; // Optional

    cert.printSubsectionHeader("BUILD DESCRIPTION VALIDATION");
    TestResult test{"Build Description Semantic Validation", TestStatus::PASS, {}};

    xmlDocPtr doc = xmlReadFile(build_desc_path.string().c_str(), nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Failed to parse 'sources/buildDescription.xml'.");
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    // Check fmiVersion in buildDescription.xml
    xmlNodePtr root = xmlDocGetRootElement(doc);
    auto bd_fmi_version = getXmlAttribute(root, "fmiVersion");
    if (bd_fmi_version)
    {
        // For FMI 2.0, buildDescription.xml (backported) should probably have fmiVersion="2.0" or "3.0"?
        // Actually, the backport says it's from FMI 3.0.
        // But usually it should match the FMU's version.
        // I'll skip strict version matching for now as it might be ambiguous for backports.
    }

    xmlXPathContextPtr xpath_context = xmlXPathNewContext(doc);
    std::set<std::string> listed_files;
    if (xpath_context)
    {
        // 1. Check SourceFile existence
        xmlXPathObjectPtr sources_xpath =
            xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//SourceFile"), xpath_context);
        if (sources_xpath && sources_xpath->nodesetval)
        {
            for (int i = 0; i < sources_xpath->nodesetval->nodeNr; ++i)
            {
                auto node =
                    sources_xpath->nodesetval->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                auto name_opt = getXmlAttribute(node, "name");
                if (name_opt)
                {
                    listed_files.insert(*name_opt);
                    auto file_path = path / "sources" / (*name_opt);
                    if (!std::filesystem::exists(file_path))
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back(
                            "Source file '" + (*name_opt) + "' listed in 'buildDescription.xml' (line " +
                            std::to_string(node->line) + ") does not exist in 'sources/' directory.");
                    }
                }
            }
        }
        if (sources_xpath)
            xmlXPathFreeObject(sources_xpath);

        // 2. Check IncludeDirectory existence
        xmlXPathObjectPtr includes_xpath =
            xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//IncludeDirectory"), xpath_context);
        if (includes_xpath && includes_xpath->nodesetval)
        {
            for (int i = 0; i < includes_xpath->nodesetval->nodeNr; ++i)
            {
                auto node =
                    includes_xpath->nodesetval->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                auto name_opt = getXmlAttribute(node, "name");
                if (name_opt)
                {
                    auto dir_path = path / "sources" / (*name_opt);
                    if (!std::filesystem::exists(dir_path) || !std::filesystem::is_directory(dir_path))
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("Include directory '" + (*name_opt) +
                                                "' listed in 'buildDescription.xml' (line " +
                                                std::to_string(node->line) +
                                                ") does not exist or is not a directory in 'sources/' directory.");
                    }
                }
            }
        }
        if (includes_xpath)
            xmlXPathFreeObject(includes_xpath);

        xmlXPathFreeContext(xpath_context);
    }

    // Reverse check: check if every source file on disk is listed
    if (std::filesystem::exists(path / "sources"))
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path / "sources"))
        {
            if (entry.is_regular_file())
            {
                auto rel_path = std::filesystem::relative(entry.path(), path / "sources");
                std::string filename = rel_path.string();
                std::replace(filename.begin(), filename.end(), '\\', '/'); // Normalize paths

                if (filename == "buildDescription.xml")
                    continue;

                // Only check typical source files
                std::string ext = entry.path().extension().string();
                for (auto& c : ext)
                    c = static_cast<char>(std::tolower(c));

                if (ext == ".c" || ext == ".cpp" || ext == ".cxx" || ext == ".cc")
                {
                    if (!listed_files.contains(filename))
                    {
                        if (test.status == TestStatus::PASS)
                            test.status = TestStatus::WARNING;
                        test.messages.push_back("Source file '" + filename +
                                                "' exists in 'sources/' directory but is not listed in "
                                                "'buildDescription.xml'.");
                    }
                }
            }
        }
    }

    xmlFreeDoc(doc);
    cert.printTestResult(test);
    cert.printSubsectionSummary(test.status != TestStatus::FAIL);
}

std::optional<std::string> BuildDescriptionChecker::getXmlAttribute(xmlNodePtr node, const std::string& attr_name)
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
