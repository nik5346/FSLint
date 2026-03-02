#include "fmi1_binary_checker.h"

#include "binary_parser.h"
#include "certificate.h"

#include <libxml/parser.h>
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>

#include <filesystem>
#include <format>
#include <set>
#include <string>
#include <vector>

void Fmi1BinaryChecker::validate(const std::filesystem::path& path, Certificate& cert)
{
    cert.printSubsectionHeader("BINARY EXPORTS");

    auto model_desc_path = path / "modelDescription.xml";
    if (!std::filesystem::exists(model_desc_path))
    {
        cert.printSubsectionSummary(false);
        return;
    }

    xmlDocPtr doc = xmlReadFile(model_desc_path.string().c_str(), nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
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
            const std::string platform = platform_entry.path().filename().string();

            for (const auto& ext : {".dll", ".so", ".dylib"})
            {
                auto binary_file = platform_entry.path() / (model_id + ext);
                if (std::filesystem::exists(binary_file))
                {
                    TestResult test{
                        std::format("Exported Functions: {}/{}{}", platform, model_id, ext), TestStatus::PASS, {}};
                    const std::set<std::string> actual_exports = BinaryParser::getExports(binary_file);

                    for (const auto& func : base_functions)
                    {
                        const std::string prefixed_func = std::format("{}_{}", model_id, func);
                        if (!actual_exports.contains(prefixed_func))
                        {
                            test.status = TestStatus::FAIL;
                            test.messages.push_back("Mandatory function '" + prefixed_func + "' is not exported.");
                        }
                    }
                    cert.printTestResult(test);
                }
            }
        }
    }

    cert.printSubsectionSummary(true);
}
