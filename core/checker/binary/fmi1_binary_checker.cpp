#include "fmi1_binary_checker.h"

#include "binary_parser.h"
#include "certificate.h"
#include "file_utils.h"
#include "xml_utils.h"

#include <libxml/parser.h>
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>

#include <filesystem>
#include <format>
#include <set>
#include <string>
#include <vector>

void Fmi1BinaryChecker::validate(const std::filesystem::path& path, Certificate& cert) const
{
    cert.printSubsectionHeader("FMU BINARY CHECKS");

    auto model_desc_path = path / "modelDescription.xml";
    if (!std::filesystem::exists(model_desc_path))
    {
        cert.printSubsectionSummary(false);
        return;
    }

    xmlDocPtr doc = readXmlFile(model_desc_path);
    if (!doc)
    {
        cert.printSubsectionSummary(false);
        return;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    auto model_id_opt = getXmlAttribute(root, "modelIdentifier");
    const std::string model_id = model_id_opt.value_or("");

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
    xmlFreeDoc(doc);

    if (model_id.empty())
    {
        cert.printSubsectionSummary(true);
        return;
    }

    std::vector<std::string> base_functions;
    if (is_cs)
    {
        base_functions = {"fmiGetTypesPlatform",
                          "fmiGetVersion",
                          "fmiInstantiateSlave",
                          "fmiInitializeSlave",
                          "fmiTerminateSlave",
                          "fmiResetSlave",
                          "fmiFreeSlaveInstance",
                          "fmiSetDebugLogging",
                          "fmiSetReal",
                          "fmiSetInteger",
                          "fmiSetBoolean",
                          "fmiSetString",
                          "fmiSetRealInputDerivatives",
                          "fmiGetReal",
                          "fmiGetInteger",
                          "fmiGetBoolean",
                          "fmiGetString",
                          "fmiGetRealOutputDerivatives",
                          "fmiDoStep",
                          "fmiCancelStep",
                          "fmiGetStatus",
                          "fmiGetRealStatus",
                          "fmiGetIntegerStatus",
                          "fmiGetBooleanStatus",
                          "fmiGetStringStatus"};
    }
    else
    {
        base_functions = {"fmiGetModelTypesPlatform",
                          "fmiGetVersion",
                          "fmiInstantiateModel",
                          "fmiFreeModelInstance",
                          "fmiSetDebugLogging",
                          "fmiSetTime",
                          "fmiSetContinuousStates",
                          "fmiCompletedIntegratorStep",
                          "fmiSetReal",
                          "fmiSetInteger",
                          "fmiSetBoolean",
                          "fmiSetString",
                          "fmiInitialize",
                          "fmiGetDerivatives",
                          "fmiGetEventIndicators",
                          "fmiGetReal",
                          "fmiGetInteger",
                          "fmiGetBoolean",
                          "fmiGetString",
                          "fmiEventUpdate",
                          "fmiGetContinuousStates",
                          "fmiGetNominalContinuousStates",
                          "fmiGetStateValueReferences",
                          "fmiTerminate"};
    }

    auto binaries_path = path / "binaries";
    if (std::filesystem::exists(binaries_path))
    {
        for (const auto& platform_entry : std::filesystem::directory_iterator(binaries_path))
        {
            if (!platform_entry.is_directory())
                continue;
            const std::string platform = file_utils::pathToUtf8(platform_entry.path().filename());

            for (const auto& ext : {".dll", ".so", ".dylib"})
            {
                auto binary_file = platform_entry.path() / (model_id + ext);
                if (std::filesystem::exists(binary_file))
                {
                    const BinaryInfo info = BinaryParser::parse(binary_file);

                    // 1. Exported Functions Check
                    TestResult export_test{
                        std::format("Exported Functions: {}/{}{}", platform, model_id, ext), TestStatus::PASS, {}};

                    for (const auto& func : base_functions)
                    {
                        const std::string prefixed_func = std::format("{}_{}", model_id, func);
                        if (!info.exports.contains(prefixed_func))
                        {
                            export_test.status = TestStatus::FAIL;
                            export_test.messages.push_back("Mandatory function '" + prefixed_func +
                                                           "' is not exported.");
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
                            format_test.messages.push_back("Binary format is not PE (Windows).");
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
                            format_test.messages.push_back("Binary format is not ELF (Linux).");
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
                            format_test.messages.push_back("Binary format is not Mach-O (macOS).");
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

                    cert.printTestResult(format_test);
                }
            }
        }
    }

    cert.printSubsectionSummary(true);
}
