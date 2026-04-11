#include "binary_checker.h"

#include "binary_parser.h"
#include "certificate.h"
#include "file_utils.h"
#include "xml_utils.h"

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>

#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

void BinaryChecker::validate(const std::filesystem::path& path, Certificate& cert) const
{
    const std::filesystem::path binaries_path = path / "binaries";
    if (!std::filesystem::exists(binaries_path))
        return;

    const std::filesystem::path model_desc_path = path / "modelDescription.xml";
    if (!std::filesystem::exists(model_desc_path))
        return;

    xmlDocPtr doc = readXmlFile(model_desc_path);
    if (doc == nullptr)
    {
        cert.printSubsectionSummary(false);
        return;
    }

    std::map<std::string, std::set<InterfaceType>> model_id_to_interfaces;
    const std::vector<std::pair<std::string, InterfaceType>> interface_mappings = {
        {"CoSimulation", InterfaceType::CO_SIMULATION},
        {"ModelExchange", InterfaceType::MODEL_EXCHANGE},
        {"ScheduledExecution", InterfaceType::SCHEDULED_EXECUTION}};

    xmlXPathContextPtr xpath_context = xmlXPathNewContext(doc);
    if (xpath_context != nullptr)
    {
        for (const auto& [elem, type] : interface_mappings)
        {
            const std::string xpath_query = "//" + elem;
            const xmlXPathObjectPtr xpath_obj =
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(xpath_query.c_str()), xpath_context);
            if (xpath_obj != nullptr && xpath_obj->nodesetval != nullptr)
            {
                for (int i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
                {
                    const std::optional<std::string> model_id =
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                        getXmlAttribute(xpath_obj->nodesetval->nodeTab[i], "modelIdentifier");
                    if (model_id.has_value())
                        model_id_to_interfaces[*model_id].insert(type);
                }
            }
            if (xpath_obj != nullptr)
                xmlXPathFreeObject(xpath_obj);
        }
        xmlXPathFreeContext(xpath_context);
    }
    xmlFreeDoc(doc);

    if (model_id_to_interfaces.empty())
        return;

    bool header_printed = false;
    auto ensure_header = [&]()
    {
        if (!header_printed)
        {
            cert.printSubsectionHeader("FMU BINARY CHECKS");
            header_printed = true;
        }
    };

    for (const auto& platform_entry : std::filesystem::directory_iterator(binaries_path))
    {
        if (!platform_entry.is_directory())
            continue;

        const std::string platform = file_utils::pathToUtf8(platform_entry.path().filename());

        for (const auto& [model_id, interfaces] : model_id_to_interfaces)
        {
            const std::vector<std::string> extensions = {".dll", ".so", ".dylib"};
            for (const auto& ext : extensions)
            {
                const auto binary_file = platform_entry.path() / (model_id + ext);
                if (std::filesystem::exists(binary_file))
                {
                    ensure_header();
                    const BinaryInfo info = BinaryParser::parse(binary_file);

                    // 1. Exported Functions Check
                    TestResult export_test{
                        std::format("Exported Functions: {}/{}{}", platform, model_id, ext), TestStatus::PASS, {}};

                    const std::vector<std::string> expected_functions = getExpectedFunctions(interfaces);

                    for (const auto& func : expected_functions)
                    {
                        if (!info.exports.contains(func))
                        {
                            export_test.setStatus(TestStatus::FAIL);
                            export_test.getMessages().emplace_back(
                                std::format("Mandatory function '{}' is not exported.", func));
                        }
                    }
                    cert.printTestResult(export_test);
                    if (cert.shouldAbort())
                        return;

                    // 2. Binary Format, Bitness, and Architecture Check
                    TestResult format_test{
                        std::format("Binary Format: {}/{}{}", platform, model_id, ext), TestStatus::PASS, {}};

                    if (!info.isSharedLibrary && info.format != BinaryFormat::UNKNOWN)
                    {
                        format_test.setStatus(TestStatus::FAIL);
                        format_test.getMessages().emplace_back("Binary is not a shared library (DLL/SO/DYLIB).");
                    }

                    if (platform.starts_with("win"))
                    {
                        if (info.format != BinaryFormat::PE)
                        {
                            format_test.setStatus(TestStatus::FAIL);
                            format_test.getMessages().emplace_back(
                                std::format("Binary format is not PE (Windows), found {}",
                                            info.format == BinaryFormat::ELF     ? "ELF"
                                            : info.format == BinaryFormat::MACHO ? "Mach-O"
                                                                                 : "unknown"));
                        }

                        bool arch_match = false;
                        for (const auto& arch : info.architectures)
                        {
                            if (arch.architecture == "x86" || arch.architecture == "x86_64")
                            {
                                if ((platform.ends_with("32") && arch.bitness == 32) ||
                                    (platform.ends_with("64") && arch.bitness == 64))
                                {
                                    arch_match = true;
                                    break;
                                }
                            }
                        }

                        if (!arch_match && !info.architectures.empty())
                        {
                            format_test.setStatus(TestStatus::FAIL);
                            format_test.getMessages().emplace_back(
                                std::format("Binary does not contain a {} architecture matching platform '{}'.",
                                            platform.ends_with("32") ? "32-bit x86" : "64-bit x86_64", platform));
                        }
                    }
                    else if (platform.starts_with("linux"))
                    {
                        if (info.format != BinaryFormat::ELF)
                        {
                            format_test.setStatus(TestStatus::FAIL);
                            format_test.getMessages().emplace_back(
                                std::format("Binary format is not ELF (Linux), found {}",
                                            info.format == BinaryFormat::PE      ? "PE"
                                            : info.format == BinaryFormat::MACHO ? "Mach-O"
                                                                                 : "unknown"));
                        }

                        bool arch_match = false;
                        for (const auto& arch : info.architectures)
                        {
                            if (arch.architecture == "x86" || arch.architecture == "x86_64")
                            {
                                if ((platform.ends_with("32") && arch.bitness == 32) ||
                                    (platform.ends_with("64") && arch.bitness == 64))
                                {
                                    arch_match = true;
                                    break;
                                }
                            }
                        }

                        if (!arch_match && !info.architectures.empty())
                        {
                            format_test.setStatus(TestStatus::FAIL);
                            format_test.getMessages().emplace_back(
                                std::format("Binary does not contain a {} architecture matching platform '{}'.",
                                            platform.ends_with("32") ? "32-bit x86" : "64-bit x86_64", platform));
                        }
                    }
                    else if (platform.starts_with("darwin"))
                    {
                        if (info.format != BinaryFormat::MACHO)
                        {
                            format_test.setStatus(TestStatus::FAIL);
                            format_test.getMessages().emplace_back(
                                std::format("Binary format is not Mach-O (macOS), found {}",
                                            info.format == BinaryFormat::PE    ? "PE"
                                            : info.format == BinaryFormat::ELF ? "ELF"
                                                                               : "unknown"));
                        }

                        bool arch_match = false;
                        for (const auto& arch : info.architectures)
                        {
                            if (arch.architecture == "x86" || arch.architecture == "x86_64")
                            {
                                if ((platform.ends_with("32") && arch.bitness == 32) ||
                                    (platform.ends_with("64") && arch.bitness == 64))
                                {
                                    arch_match = true;
                                    break;
                                }
                            }
                        }

                        if (!arch_match && !info.architectures.empty())
                        {
                            format_test.setStatus(TestStatus::FAIL);
                            format_test.getMessages().emplace_back(
                                std::format("Binary does not contain a {} architecture matching platform '{}'.",
                                            platform.ends_with("32") ? "32-bit x86" : "64-bit x86_64", platform));
                        }
                    }
                    else if (platform.find('-') != std::string::npos) // FMI 3.0 platform tuples
                    {
                        // x86_64-windows, aarch64-linux, etc.
                        if (platform.find("windows") != std::string::npos && info.format != BinaryFormat::PE)
                        {
                            format_test.setStatus(TestStatus::FAIL);
                            format_test.getMessages().emplace_back("Binary format is not PE (Windows).");
                        }
                        else if (platform.find("linux") != std::string::npos && info.format != BinaryFormat::ELF)
                        {
                            format_test.setStatus(TestStatus::FAIL);
                            format_test.getMessages().emplace_back("Binary format is not ELF (Linux).");
                        }
                        else if (platform.find("darwin") != std::string::npos && info.format != BinaryFormat::MACHO)
                        {
                            format_test.setStatus(TestStatus::FAIL);
                            format_test.getMessages().emplace_back("Binary format is not Mach-O (macOS).");
                        }

                        bool arch_match = false;
                        for (const auto& arch : info.architectures)
                        {
                            // Robust architecture check: ensure the architecture is an exact component
                            // (e.g., 'x86' should not match 'x86_64-windows').
                            if (platform.starts_with(arch.architecture + "-"))
                            {
                                arch_match = true;
                                break;
                            }
                        }

                        if (!arch_match && !info.architectures.empty())
                        {
                            format_test.setStatus(TestStatus::FAIL);
                            format_test.getMessages().emplace_back(std::format(
                                "Binary does not contain an architecture matching platform tuple '{}'.", platform));
                        }
                    }

                    cert.printTestResult(format_test);
                    if (cert.shouldAbort())
                        return;
                }
            }
        }
    }

    if (header_printed)
        cert.printSubsectionSummary(true);
}

std::optional<std::string> BinaryChecker::getXmlAttribute(xmlNodePtr node, const std::string& attr_name)
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
