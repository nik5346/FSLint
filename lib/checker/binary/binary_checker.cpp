#include "binary_checker.h"
#include "binary_parser.h"
#include "certificate.h"
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <filesystem>
#include <algorithm>
#include <map>
#include <set>

void BinaryChecker::validate(const std::filesystem::path& path, Certificate& cert)
{
    cert.printSubsectionHeader("FMU BINARY EXPORTS");

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

    std::string fmi_version;
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (root)
    {
        auto version_opt = getXmlAttribute(root, "fmiVersion");
        if (version_opt)
            fmi_version = *version_opt;
    }

    if (fmi_version.empty())
    {
        xmlFreeDoc(doc);
        cert.printSubsectionSummary(false);
        return;
    }

    std::map<std::string, std::string> model_identifiers;
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
                {
                    model_identifiers[elem] = *model_id;
                }
            }
            if (xpath_obj)
                xmlXPathFreeObject(xpath_obj);
        }
        xmlXPathFreeContext(xpath_context);
    }
    xmlFreeDoc(doc);

    if (model_identifiers.empty())
    {
        cert.printSubsectionSummary(true);
        return;
    }

    std::vector<std::string> expected_functions = getExpectedFunctions(fmi_version);

    auto binaries_path = path / "binaries";
    if (std::filesystem::exists(binaries_path))
    {
        for (const auto& platform_entry : std::filesystem::directory_iterator(binaries_path))
        {
            if (!platform_entry.is_directory())
                continue;

            std::string platform = platform_entry.path().filename().string();

            for (const auto& [interface, model_id] : model_identifiers)
            {
                std::vector<std::string> extensions = {".dll", ".so", ".dylib"};
                for (const auto& ext : extensions)
                {
                    auto binary_file = platform_entry.path() / (model_id + ext);
                    if (std::filesystem::exists(binary_file))
                    {
                        TestResult test{"Exported Functions: " + platform + "/" + model_id + ext, TestStatus::PASS, {}};
                        std::set<std::string> actual_exports = BinaryParser::getExports(binary_file);

                        for (const auto& func : expected_functions)
                        {
                            if (!actual_exports.contains(func))
                            {
                                test.status = TestStatus::FAIL;
                                test.messages.push_back("Mandatory function '" + func + "' is not exported.");
                            }
                        }

                        cert.printTestResult(test);
                    }
                }
            }
        }
    }

    cert.printSubsectionSummary(true);
}

std::vector<std::string> BinaryChecker::getExpectedFunctions(const std::string& version)
{
    if (version.starts_with("2."))
    {
        return {"fmi2GetTypesPlatform",
                "fmi2GetVersion",
                "fmi2SetDebugLogging",
                "fmi2Instantiate",
                "fmi2FreeInstance",
                "fmi2SetupExperiment",
                "fmi2EnterInitializationMode",
                "fmi2ExitInitializationMode",
                "fmi2Terminate",
                "fmi2Reset",
                "fmi2GetReal",
                "fmi2GetInteger",
                "fmi2GetBoolean",
                "fmi2GetString",
                "fmi2SetReal",
                "fmi2SetInteger",
                "fmi2SetBoolean",
                "fmi2SetString",
                "fmi2GetFMUstate",
                "fmi2SetFMUstate",
                "fmi2FreeFMUstate",
                "fmi2SerializedFMUstateSize",
                "fmi2SerializeFMUstate",
                "fmi2DeSerializeFMUstate",
                "fmi2GetDirectionalDerivative",
                "fmi2EnterEventMode",
                "fmi2NewDiscreteStates",
                "fmi2EnterContinuousTimeMode",
                "fmi2CompletedIntegratorStep",
                "fmi2SetTime",
                "fmi2SetContinuousStates",
                "fmi2GetDerivatives",
                "fmi2GetEventIndicators",
                "fmi2GetContinuousStates",
                "fmi2GetNominalsOfContinuousStates",
                "fmi2SetRealInputDerivatives",
                "fmi2GetRealOutputDerivatives",
                "fmi2DoStep",
                "fmi2CancelStep",
                "fmi2GetStatus",
                "fmi2GetRealStatus",
                "fmi2GetIntegerStatus",
                "fmi2GetBooleanStatus",
                "fmi2GetStringStatus"};
    }
    else if (version.starts_with("3."))
    {
        return {"fmi3GetVersion",
                "fmi3SetDebugLogging",
                "fmi3InstantiateModelExchange",
                "fmi3InstantiateCoSimulation",
                "fmi3InstantiateScheduledExecution",
                "fmi3FreeInstance",
                "fmi3EnterInitializationMode",
                "fmi3ExitInitializationMode",
                "fmi3EnterEventMode",
                "fmi3Terminate",
                "fmi3Reset",
                "fmi3GetFloat32",
                "fmi3GetFloat64",
                "fmi3GetInt8",
                "fmi3GetUInt8",
                "fmi3GetInt16",
                "fmi3GetUInt16",
                "fmi3GetInt32",
                "fmi3GetUInt32",
                "fmi3GetInt64",
                "fmi3GetUInt64",
                "fmi3GetBoolean",
                "fmi3GetString",
                "fmi3GetBinary",
                "fmi3GetClock",
                "fmi3SetFloat32",
                "fmi3SetFloat64",
                "fmi3SetInt8",
                "fmi3SetUInt8",
                "fmi3SetInt16",
                "fmi3SetUInt16",
                "fmi3SetInt32",
                "fmi3SetUInt32",
                "fmi3SetInt64",
                "fmi3SetUInt64",
                "fmi3SetBoolean",
                "fmi3SetString",
                "fmi3SetBinary",
                "fmi3SetClock",
                "fmi3GetNumberOfVariableDependencies",
                "fmi3GetVariableDependencies",
                "fmi3GetFMUState",
                "fmi3SetFMUState",
                "fmi3FreeFMUState",
                "fmi3SerializedFMUStateSize",
                "fmi3SerializeFMUState",
                "fmi3DeserializeFMUState",
                "fmi3GetDirectionalDerivative",
                "fmi3GetAdjointDerivative",
                "fmi3EnterConfigurationMode",
                "fmi3ExitConfigurationMode",
                "fmi3GetIntervalDecimal",
                "fmi3GetIntervalFraction",
                "fmi3GetShiftDecimal",
                "fmi3GetShiftFraction",
                "fmi3SetIntervalDecimal",
                "fmi3SetIntervalFraction",
                "fmi3SetShiftDecimal",
                "fmi3SetShiftFraction",
                "fmi3EvaluateDiscreteStates",
                "fmi3UpdateDiscreteStates",
                "fmi3EnterContinuousTimeMode",
                "fmi3CompletedIntegratorStep",
                "fmi3SetTime",
                "fmi3SetContinuousStates",
                "fmi3GetContinuousStateDerivatives",
                "fmi3GetEventIndicators",
                "fmi3GetContinuousStates",
                "fmi3GetNominalsOfContinuousStates",
                "fmi3GetNumberOfEventIndicators",
                "fmi3GetNumberOfContinuousStates",
                "fmi3EnterStepMode",
                "fmi3GetOutputDerivatives",
                "fmi3DoStep",
                "fmi3ActivateModelPartition"};
    }
    return {};
}

std::optional<std::string> BinaryChecker::getXmlAttribute(xmlNodePtr node, const std::string& attr_name)
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
