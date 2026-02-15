#include "build_description_checker.h"

#include "certificate.h"

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xmlstring.h>
#include <libxml/tree.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

void BuildDescriptionChecker::validate(const std::filesystem::path& path, Certificate& cert)
{
    auto sources_path = path / "sources";
    auto build_desc_path = sources_path / "buildDescription.xml";
    if (!std::filesystem::exists(build_desc_path))
        return; // Optional

    cert.printSubsectionHeader("BUILD DESCRIPTION VALIDATION");

    xmlDocPtr doc = xmlReadFile(build_desc_path.string().c_str(), nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc)
    {
        TestResult test{
            "Parse buildDescription.xml", TestStatus::FAIL, {"Failed to parse 'sources/buildDescription.xml'."}};
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    checkFmiVersion(root, cert);

    xmlXPathContextPtr xpath_context = xmlXPathNewContext(doc);
    std::set<std::string> listed_files;
    if (xpath_context)
    {
        auto valid_ids = getValidModelIdentifiers(path);
        checkBuildConfigurationAttributes(xpath_context, valid_ids, cert);
        checkSourceFiles(xpath_context, sources_path, cert, listed_files);
        checkIncludeDirectories(xpath_context, sources_path, cert);
        xmlXPathFreeContext(xpath_context);
    }

    // Reverse check: check if every source file on disk is listed
    {
        TestResult test{"Build Description Consistency", TestStatus::PASS, {}};
        if (std::filesystem::exists(sources_path))
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(sources_path))
            {
                if (entry.is_regular_file())
                {
                    auto rel_path = std::filesystem::relative(entry.path(), sources_path);
                    std::string filename = rel_path.string();
                    std::replace(filename.begin(), filename.end(), '\\', '/'); // Normalize paths

                    if (filename == "buildDescription.xml")
                        continue;

                    // Only check typical source files
                    static const std::set<std::string> source_extensions = {".c", ".cc", ".cpp", ".cxx", ".C", ".c++"};
                    std::string ext = entry.path().extension().string();

                    if (source_extensions.contains(ext))
                    {
                        if (!listed_files.contains(filename))
                        {
                            test.status = TestStatus::WARNING;
                            test.messages.push_back("Source file '" + filename +
                                                    "' exists in 'sources/' directory but is not listed in "
                                                    "'buildDescription.xml'.");
                        }
                    }
                }
            }
        }
        cert.printTestResult(test);
    }

    xmlFreeDoc(doc);
    cert.printSubsectionSummary(true);
}

void BuildDescriptionChecker::checkSourceFiles(xmlXPathContextPtr xpath_context,
                                               const std::filesystem::path& sources_path, Certificate& cert,
                                               std::set<std::string>& listed_files)
{
    TestResult test{"Build Description Source Files", TestStatus::PASS, {}};
    xmlXPathObjectPtr sources_xpath =
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//SourceFile"), xpath_context);
    if (sources_xpath && sources_xpath->nodesetval)
    {
        for (int i = 0; i < sources_xpath->nodesetval->nodeNr; ++i)
        {
            auto node = sources_xpath->nodesetval->nodeTab[i];
            auto name_opt = getXmlAttribute(node, "name");
            if (name_opt)
            {
                if (name_opt->find("..") != std::string::npos)
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Source file '" + (*name_opt) +
                                            "' listed in 'buildDescription.xml' (line " + std::to_string(node->line) +
                                            ") contains illegal '..' sequence.");
                    continue;
                }

                listed_files.insert(*name_opt);
                auto file_path = sources_path / (*name_opt);
                if (!std::filesystem::exists(file_path))
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Source file '" + (*name_opt) +
                                            "' listed in 'buildDescription.xml' (line " + std::to_string(node->line) +
                                            ") does not exist in 'sources/' directory.");
                }
            }
        }
    }
    if (sources_xpath)
        xmlXPathFreeObject(sources_xpath);
    cert.printTestResult(test);
}

void BuildDescriptionChecker::checkIncludeDirectories(xmlXPathContextPtr xpath_context,
                                                      const std::filesystem::path& sources_path, Certificate& cert)
{
    TestResult test{"Build Description Include Directories", TestStatus::PASS, {}};
    xmlXPathObjectPtr includes_xpath =
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//IncludeDirectory"), xpath_context);
    if (includes_xpath && includes_xpath->nodesetval)
    {
        for (int i = 0; i < includes_xpath->nodesetval->nodeNr; ++i)
        {
            auto node = includes_xpath->nodesetval->nodeTab[i];
            auto name_opt = getXmlAttribute(node, "name");
            if (name_opt)
            {
                if (name_opt->find("..") != std::string::npos)
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Include directory '" + (*name_opt) +
                                            "' listed in 'buildDescription.xml' (line " + std::to_string(node->line) +
                                            ") contains illegal '..' sequence.");
                    continue;
                }

                auto dir_path = sources_path / (*name_opt);
                if (!std::filesystem::exists(dir_path) || !std::filesystem::is_directory(dir_path))
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("Include directory '" + (*name_opt) +
                                            "' listed in 'buildDescription.xml' (line " + std::to_string(node->line) +
                                            ") does not exist or is not a directory in 'sources/' directory.");
                }
            }
        }
    }
    if (includes_xpath)
        xmlXPathFreeObject(includes_xpath);
    cert.printTestResult(test);
}

void BuildDescriptionChecker::checkBuildConfigurationAttributes(xmlXPathContextPtr xpath_context,
                                                                const std::set<std::string>& valid_ids,
                                                                Certificate& cert)
{
    TestResult test{"Build Configuration Attributes", TestStatus::PASS, {}};
    xmlXPathObjectPtr configs_xpath =
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//BuildConfiguration"), xpath_context);
    if (configs_xpath && configs_xpath->nodesetval)
    {
        // Suggested in FMI 3.0: gcc, clang++
        // Suggested in FMI 3.0: C99, C++11
        static const std::set<std::string> suggested_languages = {"C89",   "C90",   "C99",   "C11",   "C17",
                                                                  "C18",   "C23",   "C++98", "C++03", "C++11",
                                                                  "C++14", "C++17", "C++20", "C++23", "C++26"};
        static const std::set<std::string> suggested_compilers = {"gcc", "clang", "msvc"};

        for (int i = 0; i < configs_xpath->nodesetval->nodeNr; ++i)
        {
            auto node = configs_xpath->nodesetval->nodeTab[i];

            auto model_id = getXmlAttribute(node, "modelIdentifier");
            if (model_id && !valid_ids.empty())
            {
                if (!valid_ids.contains(*model_id))
                {
                    test.status = TestStatus::FAIL;
                    test.messages.push_back("BuildConfiguration (line " + std::to_string(node->line) +
                                            ") has modelIdentifier '" + *model_id +
                                            "' which does not match any modelIdentifier in modelDescription.xml.");
                }
            }

            auto lang_opt = getXmlAttribute(node, "language");
            if (lang_opt)
            {
                if (!suggested_languages.contains(*lang_opt))
                {
                    if (test.status == TestStatus::PASS)
                        test.status = TestStatus::WARNING;
                    test.messages.push_back("Language '" + *lang_opt + "' in BuildConfiguration (line " +
                                            std::to_string(node->line) +
                                            ") is not one of the suggested values (e.g. C99, C++11).");
                }
            }

            auto compiler_opt = getXmlAttribute(node, "compiler");
            if (compiler_opt)
            {
                bool known = false;
                for (const auto& suggested : suggested_compilers)
                {
                    if (compiler_opt->find(suggested) != std::string::npos)
                    {
                        known = true;
                        break;
                    }
                }
                if (!known)
                {
                    if (test.status == TestStatus::PASS)
                        test.status = TestStatus::WARNING;
                    test.messages.push_back("Compiler '" + *compiler_opt + "' in BuildConfiguration (line " +
                                            std::to_string(node->line) +
                                            ") is not one of the suggested values (e.g. gcc, clang, msvc).");
                }
            }
        }
    }
    if (configs_xpath)
        xmlXPathFreeObject(configs_xpath);
    cert.printTestResult(test);
}

std::set<std::string> BuildDescriptionChecker::getValidModelIdentifiers(const std::filesystem::path& path)
{
    std::set<std::string> ids;
    auto md_path = path / "modelDescription.xml";
    if (!std::filesystem::exists(md_path))
        return ids;

    xmlDocPtr doc = xmlReadFile(md_path.string().c_str(), nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc)
        return ids;

    xmlXPathContextPtr xpath_context = xmlXPathNewContext(doc);
    if (xpath_context)
    {
        // FMI2 and FMI3 tags
        static const std::vector<std::string> tags = {"ModelExchange", "CoSimulation", "ScheduledExecution"};
        for (const auto& tag : tags)
        {
            std::string expr = "//" + tag;
            xmlXPathObjectPtr xpath_obj =
                xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.c_str()), xpath_context);
            if (xpath_obj && xpath_obj->nodesetval)
            {
                for (int i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
                {
                    auto node = xpath_obj->nodesetval->nodeTab[i];
                    auto id = getXmlAttribute(node, "modelIdentifier");
                    if (id)
                        ids.insert(*id);
                }
            }
            if (xpath_obj)
                xmlXPathFreeObject(xpath_obj);
        }
        xmlXPathFreeContext(xpath_context);
    }
    xmlFreeDoc(doc);
    return ids;
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
