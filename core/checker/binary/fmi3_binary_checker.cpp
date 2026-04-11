#include "fmi3_binary_checker.h"

#include <string>
#include <vector>

std::vector<std::string> Fmi3BinaryChecker::getExpectedFunctions(const std::set<InterfaceType>& interfaces) const
{
    std::vector<std::string> functions = {"fmi3GetVersion",
                                          "fmi3SetDebugLogging",
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
                                          "fmi3UpdateDiscreteStates"};

    if (interfaces.contains(InterfaceType::MODEL_EXCHANGE))
    {
        const std::vector<std::string> me_functions = {"fmi3InstantiateModelExchange",
                                                       "fmi3EnterContinuousTimeMode",
                                                       "fmi3CompletedIntegratorStep",
                                                       "fmi3SetTime",
                                                       "fmi3SetContinuousStates",
                                                       "fmi3GetContinuousStateDerivatives",
                                                       "fmi3GetEventIndicators",
                                                       "fmi3GetContinuousStates",
                                                       "fmi3GetNominalsOfContinuousStates",
                                                       "fmi3GetNumberOfEventIndicators",
                                                       "fmi3GetNumberOfContinuousStates"};
        functions.insert(functions.end(), me_functions.begin(), me_functions.end());
    }

    if (interfaces.contains(InterfaceType::CO_SIMULATION))
    {
        const std::vector<std::string> cs_functions = {"fmi3InstantiateCoSimulation", "fmi3EnterStepMode",
                                                       "fmi3GetOutputDerivatives", "fmi3DoStep"};
        functions.insert(functions.end(), cs_functions.begin(), cs_functions.end());
    }

    if (interfaces.contains(InterfaceType::SCHEDULED_EXECUTION))
    {
        const std::vector<std::string> se_functions = {"fmi3InstantiateScheduledExecution",
                                                       "fmi3ActivateModelPartition"};
        functions.insert(functions.end(), se_functions.begin(), se_functions.end());
    }

    return functions;
}
