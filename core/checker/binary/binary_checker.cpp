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
#include <optional>
#include <set>
#include <string>
#include <vector>

void BinaryChecker::validate(const std::filesystem::path& path, Certificate& cert) const
{
    auto binaries_path = path / "binaries";
    if (!std::filesystem::exists(binaries_path))
        return;

    auto model_desc_path = path / "modelDescription.xml";
    if (!std::filesystem::exists(model_desc_path))
        return;

    xmlDocPtr doc = readXmlFile(model_desc_path);
    if (!doc)
    {
        cert.printSubsectionSummary(false);
        return;
    }

    std::set<std::string> model_identifiers;
    const std::vector<std::string> interface_elements = {"CoSimulation", "ModelExchange", "ScheduledExecution"};

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
                auto model_id = getXmlAttribute(xpath_obj->nodesetval->nodeTab[0], "modelIdentifier");
                if (model_id)
                    model_identifiers.insert(*model_id);
            }
            if (xpath_obj)
                xmlXPathFreeObject(xpath_obj);
        }
        xmlXPathFreeContext(xpath_context);
    }
    xmlFreeDoc(doc);

    if (model_identifiers.empty())
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

    const std::vector<std::string> expected_functions = getExpectedFunctions();

    for (const auto& platform_entry : std::filesystem::directory_iterator(binaries_path))
    {
        if (!platform_entry.is_directory())
            continue;

        const std::string platform = file_utils::pathToUtf8(platform_entry.path().filename());

        for (const auto& model_id : model_identifiers)
        {
            const std::vector<std::string> extensions = {".dll", ".so", ".dylib"};
            for (const auto& ext : extensions)
            {
                auto binary_file = platform_entry.path() / (model_id + ext);
                if (std::filesystem::exists(binary_file))
                {
                    ensure_header();
                    const BinaryInfo info = BinaryParser::parse(binary_file);

                    // 1. Exported Functions Check
                    TestResult export_test{
                        std::format("Exported Functions: {}/{}{}", platform, model_id, ext), TestStatus::PASS, {}};

                    for (const auto& func : expected_functions)
                    {
                        if (!info.exports.contains(func))
                        {
                            export_test.status = TestStatus::FAIL;
                            export_test.messages.push_back("Mandatory function '" + func + "' is not exported.");
                        }
                    }
                    cert.printTestResult(export_test);

                    // 2. Binary Format, Bitness, and Architecture Check
                    TestResult format_test{
                        std::format("Binary Format: {}/{}{}", platform, model_id, ext), TestStatus::PASS, {}};

                    if (platform.starts_with("win"))
                    {
                        if (info.format != BinaryFormat::PE)
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Binary format is not PE (Windows), found {}",
                                            (info.format == BinaryFormat::ELF     ? "ELF"
                                             : info.format == BinaryFormat::MACHO ? "Mach-O"
                                                                                  : "unknown")));
                        }

                        if (info.architecture != "x86" && info.architecture != "x86_64")
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Platform '{}' requires x86 or x86_64 architecture, but found {}.",
                                            platform, info.architecture));
                        }

                        if (platform.ends_with("32") && info.bitness != 32)
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Platform 'win32' requires a 32-bit binary, but found {}-bit.",
                                            info.bitness == 0 ? "unknown" : std::to_string(info.bitness)));
                        }
                        else if (platform.ends_with("64") && info.bitness != 64)
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Platform 'win64' requires a 64-bit binary, but found {}-bit.",
                                            info.bitness == 0 ? "unknown" : std::to_string(info.bitness)));
                        }
                    }
                    else if (platform.starts_with("linux"))
                    {
                        if (info.format != BinaryFormat::ELF)
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Binary format is not ELF (Linux), found {}",
                                            (info.format == BinaryFormat::PE      ? "PE"
                                             : info.format == BinaryFormat::MACHO ? "Mach-O"
                                                                                  : "unknown")));
                        }

                        if (info.architecture != "x86" && info.architecture != "x86_64")
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Platform '{}' requires x86 or x86_64 architecture, but found {}.",
                                            platform, info.architecture));
                        }

                        if (platform.ends_with("32") && info.bitness != 32)
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Platform 'linux32' requires a 32-bit binary, but found {}-bit.",
                                            info.bitness == 0 ? "unknown" : std::to_string(info.bitness)));
                        }
                        else if (platform.ends_with("64") && info.bitness != 64)
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Platform 'linux64' requires a 64-bit binary, but found {}-bit.",
                                            info.bitness == 0 ? "unknown" : std::to_string(info.bitness)));
                        }
                    }
                    else if (platform.starts_with("darwin"))
                    {
                        if (info.format != BinaryFormat::MACHO)
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Binary format is not Mach-O (macOS), found {}",
                                            (info.format == BinaryFormat::PE    ? "PE"
                                             : info.format == BinaryFormat::ELF ? "ELF"
                                                                                : "unknown")));
                        }

                        if (info.architecture != "x86" && info.architecture != "x86_64")
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Platform '{}' requires x86 or x86_64 architecture, but found {}.",
                                            platform, info.architecture));
                        }

                        if (platform.ends_with("32") && info.bitness != 32)
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Platform 'darwin32' requires a 32-bit binary, but found {}-bit.",
                                            info.bitness == 0 ? "unknown" : std::to_string(info.bitness)));
                        }
                        else if (platform.ends_with("64") && info.bitness != 64)
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Platform 'darwin64' requires a 64-bit binary, but found {}-bit.",
                                            info.bitness == 0 ? "unknown" : std::to_string(info.bitness)));
                        }
                    }
                    else if (platform.find('-') != std::string::npos) // FMI 3.0 platform tuples
                    {
                        // x86_64-windows, aarch64-linux, etc.
                        if (platform.find("windows") != std::string::npos && info.format != BinaryFormat::PE)
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back("Binary format is not PE (Windows).");
                        }
                        else if (platform.find("linux") != std::string::npos && info.format != BinaryFormat::ELF)
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back("Binary format is not ELF (Linux).");
                        }
                        else if (platform.find("darwin") != std::string::npos && info.format != BinaryFormat::MACHO)
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back("Binary format is not Mach-O (macOS).");
                        }

                        // Robust architecture check: ensure the architecture is an exact component
                        // (e.g., 'x86' should not match 'x86_64-windows').
                        if (platform.find(info.architecture + "-") != 0)
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Platform tuple '{}' does not match binary architecture '{}'.", platform,
                                            info.architecture));
                        }

                        if (platform.starts_with("x86_64") || platform.starts_with("aarch64") ||
                            platform.starts_with("riscv64") || platform.starts_with("ppc64"))
                        {
                            if (info.bitness != 64)
                            {
                                format_test.status = TestStatus::FAIL;
                                format_test.messages.push_back(
                                    std::format("64-bit platform requires a 64-bit binary, but found {}-bit.",
                                                info.bitness == 0 ? "unknown" : std::to_string(info.bitness)));
                            }
                        }
                        else if (platform.starts_with("x86") || platform.starts_with("aarch32") ||
                                 platform.starts_with("riscv32") || platform.starts_with("ppc32"))
                        {
                            if (info.bitness != 32)
                            {
                                format_test.status = TestStatus::FAIL;
                                format_test.messages.push_back(
                                    std::format("32-bit platform requires a 32-bit binary, but found {}-bit.",
                                                info.bitness == 0 ? "unknown" : std::to_string(info.bitness)));
                            }
                        }
                    }

                    cert.printTestResult(format_test);
                }
            }
        }
    }

    if (header_printed)
        cert.printSubsectionSummary(true);
}

std::optional<std::string> BinaryChecker::getXmlAttribute(xmlNodePtr node, const std::string& attr_name)
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
