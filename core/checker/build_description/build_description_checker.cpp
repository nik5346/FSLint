#include "build_description_checker.h"
#include <format>

#include "certificate.h"
#include "file_utils.h"
#include "xml_utils.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

void BuildDescriptionChecker::validate(const std::filesystem::path& path, Certificate& cert) const
{
    const auto sources_path = path / "sources";
    const auto build_desc_path = sources_path / "buildDescription.xml";
    if (!std::filesystem::exists(build_desc_path))
        return; // Optional

    cert.printSubsectionHeader("BUILD DESCRIPTION VALIDATION");

    const xmlDocPtr doc = readXmlFile(build_desc_path);
    if (doc == nullptr)
    {
        const TestResult test{
            "Parse buildDescription.xml", TestStatus::FAIL, {"Failed to parse 'sources/buildDescription.xml'."}};
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    const xmlNodePtr root = xmlDocGetRootElement(doc);
    checkFmiVersion(root, cert);

    const xmlXPathContextPtr xpath_context = xmlXPathNewContext(doc);
    std::set<std::string> listed_files;
    if (xpath_context != nullptr)
    {
        const auto valid_ids = getValidModelIdentifiers(path);
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
                    const auto rel_path = std::filesystem::relative(entry.path(), sources_path);
                    std::string filename = file_utils::pathToUtf8(rel_path);
                    std::ranges::replace(filename, '\\', '/'); // Normalize paths

                    if (filename == "buildDescription.xml")
                        continue;

                    // Only check typical source files
                    static const std::set<std::string> source_extensions = {".c", ".cc", ".cpp", ".cxx", ".C", ".c++"};
                    const std::string ext = file_utils::pathToUtf8(entry.path().extension());

                    if (source_extensions.contains(ext))
                    {
                        if (!listed_files.contains(filename))
                        {
                            if (test.getStatus() == TestStatus::PASS)
                                test.setStatus(TestStatus::WARNING);
                            test.getMessages().emplace_back(
                                std::format("Source file '{}' exists in 'sources/' directory but is not listed in "
                                            "'buildDescription.xml'.",
                                            filename));
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
                                               std::set<std::string>& listed_files) const
{
    TestResult test{"Build Description Source Files", TestStatus::PASS, {}};
    const xmlXPathObjectPtr sources_xpath =
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//SourceFile"), xpath_context);
    if (sources_xpath != nullptr && sources_xpath->nodesetval != nullptr)
    {
        for (int i = 0; i < sources_xpath->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = sources_xpath->nodesetval->nodeTab[i];
            const auto name_opt = getXmlAttribute(node, "name");
            if (name_opt.has_value())
            {
                const auto& val = *name_opt;
                if (val.find("..") != std::string::npos)
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format(
                        "Source file '{}' listed in 'buildDescription.xml' (line {}) contains illegal '..' sequence.",
                        val, node->line));
                    continue;
                }

                listed_files.insert(val);
                const auto file_path = sources_path / val;
                if (!std::filesystem::exists(file_path))
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(std::format("Source file '{}' listed in 'buildDescription.xml' "
                                                                "(line {}) does not exist in 'sources/' directory.",
                                                                val, node->line));
                }
            }
        }
    }
    if (sources_xpath != nullptr)
        xmlXPathFreeObject(sources_xpath);
    cert.printTestResult(test);
}

void BuildDescriptionChecker::checkIncludeDirectories(xmlXPathContextPtr xpath_context,
                                                      const std::filesystem::path& sources_path,
                                                      Certificate& cert) const
{
    TestResult test{"Build Description Include Directories", TestStatus::PASS, {}};
    const xmlXPathObjectPtr includes_xpath =
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//IncludeDirectory"), xpath_context);
    if (includes_xpath != nullptr && includes_xpath->nodesetval != nullptr)
    {
        for (int i = 0; i < includes_xpath->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = includes_xpath->nodesetval->nodeTab[i];
            const auto name_opt = getXmlAttribute(node, "name");
            if (name_opt.has_value())
            {
                const auto& val = *name_opt;
                if (val.find("..") != std::string::npos)
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(
                        std::format("Include directory '{}' listed in 'buildDescription.xml' (line {}) contains "
                                    "illegal '..' sequence.",
                                    val, node->line));
                    continue;
                }

                const auto dir_path = sources_path / val;
                if (!std::filesystem::exists(dir_path) || !std::filesystem::is_directory(dir_path))
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(
                        std::format("Include directory '{}' listed in 'buildDescription.xml' (line {}) does not exist "
                                    "or is not a directory in 'sources/' directory.",
                                    val, node->line));
                }
            }
        }
    }
    if (includes_xpath != nullptr)
        xmlXPathFreeObject(includes_xpath);
    cert.printTestResult(test);
}

void BuildDescriptionChecker::checkBuildConfigurationAttributes(xmlXPathContextPtr xpath_context,
                                                                const std::set<std::string>& valid_ids,
                                                                Certificate& cert) const
{
    TestResult test{"Build Configuration Attributes", TestStatus::PASS, {}};
    const xmlXPathObjectPtr configs_xpath =
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>("//BuildConfiguration"), xpath_context);
    if (configs_xpath != nullptr && configs_xpath->nodesetval != nullptr)
    {
        // Suggested in FMI 3.0: gcc, clang++
        // Suggested in FMI 3.0: C99, C++11
        static const std::set<std::string> suggested_languages = {"C89",   "C90",   "C99",   "C11",   "C17",
                                                                  "C18",   "C23",   "C++98", "C++03", "C++11",
                                                                  "C++14", "C++17", "C++20", "C++23", "C++26"};
        static const std::set<std::string> suggested_compilers = {"gcc", "clang", "msvc"};

        for (int i = 0; i < configs_xpath->nodesetval->nodeNr; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const xmlNodePtr node = configs_xpath->nodesetval->nodeTab[i];

            const auto model_id = getXmlAttribute(node, "modelIdentifier");
            if (model_id.has_value() && !valid_ids.empty())
            {
                const auto& id_val = *model_id;
                if (!valid_ids.contains(id_val))
                {
                    test.setStatus(TestStatus::FAIL);
                    test.getMessages().emplace_back(
                        std::format("BuildConfiguration (line {}) has modelIdentifier '{}' which does not match any "
                                    "modelIdentifier in modelDescription.xml.",
                                    node->line, id_val));
                }
            }

            const auto lang_opt = getXmlAttribute(node, "language");
            if (lang_opt.has_value())
            {
                const auto& lang_val = *lang_opt;
                if (!suggested_languages.contains(lang_val))
                {
                    if (test.getStatus() == TestStatus::PASS)
                        test.setStatus(TestStatus::WARNING);
                    test.getMessages().emplace_back(std::format("Language '{}' in BuildConfiguration (line {}) is not "
                                                                "one of the suggested values (e.g. C99, C++11).",
                                                                lang_val, node->line));
                }
            }

            const auto compiler_opt = getXmlAttribute(node, "compiler");
            if (compiler_opt.has_value())
            {
                const std::string& compiler = compiler_opt.value();
                bool known = false;
                for (const auto& suggested : suggested_compilers)
                {
                    if (compiler.find(suggested) != std::string::npos)
                    {
                        known = true;
                        break;
                    }
                }
                if (!known)
                {
                    if (test.getStatus() == TestStatus::PASS)
                        test.setStatus(TestStatus::WARNING);
                    test.getMessages().emplace_back(std::format("Compiler '{}' in BuildConfiguration (line {}) is not "
                                                                "one of the suggested values (e.g. gcc, clang, msvc).",
                                                                compiler, node->line));
                }
            }
        }
    }
    if (configs_xpath != nullptr)
        xmlXPathFreeObject(configs_xpath);
    cert.printTestResult(test);
}

std::set<std::string> BuildDescriptionChecker::getValidModelIdentifiers(const std::filesystem::path& path) const
{
    std::set<std::string> ids;
    const auto md_path = path / "modelDescription.xml";
    if (!std::filesystem::exists(md_path))
        return ids;

    const xmlDocPtr doc = readXmlFile(md_path);
    if (doc == nullptr)
        return ids;

    const xmlXPathContextPtr xpath_context = xmlXPathNewContext(doc);
    if (xpath_context != nullptr)
    {
        // FMI2 and FMI3 tags
        static const std::vector<std::string> tags = {"ModelExchange", "CoSimulation", "ScheduledExecution"};
        for (const auto& tag : tags)
        {
            const std::string expr = "//" + tag;
            const xmlXPathObjectPtr xpath_obj =
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.c_str()), xpath_context);
            if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
            {
                for (int i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
                {
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    const xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i];
                    const auto id = getXmlAttribute(node, "modelIdentifier");
                    if (id.has_value())
                        ids.insert(*id);
                }
            }
            if (xpath_obj != nullptr)
                xmlXPathFreeObject(xpath_obj);
        }
        xmlXPathFreeContext(xpath_context);
    }
    xmlFreeDoc(doc);
    return ids;
}

std::optional<std::string> BuildDescriptionChecker::getXmlAttribute(xmlNodePtr node, const std::string& attr_name) const
{
    if (node == nullptr)
        return std::nullopt;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    xmlChar* attr = xmlGetProp(node, reinterpret_cast<const xmlChar*>(attr_name.c_str()));
    if (attr == nullptr)
        return std::nullopt;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    std::string value(reinterpret_cast<char*>(attr));
    xmlFree(attr);
    return value;
}
