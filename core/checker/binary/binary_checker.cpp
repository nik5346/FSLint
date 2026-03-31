#include "binary_checker.h"

#include "binary_parser.h"
#include "certificate.h"
#include "file_utils.h"
#include "xml_utils.h"

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>

#include <algorithm>
#include <cctype>
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

                    if (!info.isSharedLibrary && info.format != BinaryFormat::UNKNOWN)
                    {
                        format_test.status = TestStatus::FAIL;
                        format_test.messages.push_back("Binary is not a shared library (DLL/SO/DYLIB).");
                    }

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
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Binary does not contain a {} architecture matching platform '{}'.",
                                            (platform.ends_with("32") ? "32-bit x86" : "64-bit x86_64"), platform));
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
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Binary does not contain a {} architecture matching platform '{}'.",
                                            (platform.ends_with("32") ? "32-bit x86" : "64-bit x86_64"), platform));
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
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(
                                std::format("Binary does not contain a {} architecture matching platform '{}'.",
                                            (platform.ends_with("32") ? "32-bit x86" : "64-bit x86_64"), platform));
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

                        bool arch_match = false;
                        for (const auto& arch : info.architectures)
                        {
                            // Robust architecture check: ensure the architecture is an exact component
                            // (e.g., 'x86' should not match 'x86_64-windows').
                            if (platform.find(arch.architecture + "-") == 0)
                            {
                                arch_match = true;
                                break;
                            }
                        }

                        if (!arch_match && !info.architectures.empty())
                        {
                            format_test.status = TestStatus::FAIL;
                            format_test.messages.push_back(std::format(
                                "Binary does not contain an architecture matching platform tuple '{}'.", platform));
                        }
                    }

                    cert.printTestResult(format_test);

                    // 3. Suspicious Imports Check
                    TestResult import_test{std::format("Suspicious Binary Imports: {}/{}{}", platform, model_id, ext),
                                           TestStatus::PASS,
                                           {}};

                    const std::vector<std::string> suspicious_libs = {
                        "ws2_32.dll", "libcurl",   "libssl", "wininet.dll", "winhttp.dll", "libcsoap",
                        "libnsl",     "libresolv", "libssh", "libcrypto",   "crypt32.dll", "advapi32.dll"};

                    for (const auto& imported : info.importedLibraries)
                    {
                        std::string imported_lower = imported;
                        std::transform(imported_lower.begin(), imported_lower.end(), imported_lower.begin(), ::tolower);

                        for (const auto& suspicious : suspicious_libs)
                        {
                            std::string suspicious_lower = suspicious;
                            std::transform(suspicious_lower.begin(), suspicious_lower.end(), suspicious_lower.begin(),
                                           ::tolower);

                            if (imported_lower.find(suspicious_lower) != std::string::npos)
                            {
                                import_test.status = TestStatus::WARNING;
                                import_test.messages.push_back(
                                    std::format("[SECURITY] Binary imports suspicious library '{}' (potential "
                                                "network or security risk).",
                                                imported));
                                break;
                            }
                        }
                    }

                    cert.printTestResult(import_test);
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
