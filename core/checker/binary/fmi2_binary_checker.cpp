#include "fmi2_binary_checker.h"

#include "binary_checker.h"

#include <set>
#include <string>
#include <vector>

std::vector<std::string> Fmi2BinaryChecker::getExpectedFunctions(const std::set<InterfaceType>& interfaces) const
{
    std::vector<std::string> functions = {"fmi2GetTypesPlatform",
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
                                          "fmi2GetDirectionalDerivative"};

    if (interfaces.contains(InterfaceType::MODEL_EXCHANGE))
    {
        const std::vector<std::string> me_functions = {"fmi2EnterEventMode",
                                                       "fmi2NewDiscreteStates",
                                                       "fmi2EnterContinuousTimeMode",
                                                       "fmi2CompletedIntegratorStep",
                                                       "fmi2SetTime",
                                                       "fmi2SetContinuousStates",
                                                       "fmi2GetDerivatives",
                                                       "fmi2GetEventIndicators",
                                                       "fmi2GetContinuousStates",
                                                       "fmi2GetNominalsOfContinuousStates"};
        functions.insert(functions.end(), me_functions.begin(), me_functions.end());
    }

    if (interfaces.contains(InterfaceType::CO_SIMULATION))
    {
        const std::vector<std::string> cs_functions = {"fmi2SetRealInputDerivatives",
                                                       "fmi2GetRealOutputDerivatives",
                                                       "fmi2DoStep",
                                                       "fmi2CancelStep",
                                                       "fmi2GetStatus",
                                                       "fmi2GetRealStatus",
                                                       "fmi2GetIntegerStatus",
                                                       "fmi2GetBooleanStatus",
                                                       "fmi2GetStringStatus"};
        functions.insert(functions.end(), cs_functions.begin(), cs_functions.end());
    }

    return functions;
}
