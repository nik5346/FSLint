#pragma once

#include <filesystem>
#include <string>

/// @brief Supported standards and interface variants.
enum class ModelStandard : uint8_t
{
    FMI1_ME, ///< FMI 1.0 Model Exchange.
    FMI1_CS, ///< FMI 1.0 Co-Simulation.
    FMI2,    ///< FMI 2.0.
    FMI3,    ///< FMI 3.0.
    SSP1,    ///< SSP 1.0.
    SSP2,    ///< SSP 2.0.
    UNKNOWN  ///< Unknown.
};

/// @brief Basic metadata about a detected model.
struct ModelInfo
{
    ModelStandard standard = ModelStandard::UNKNOWN; ///< Standard.
    std::string version;                             ///< Version.
    std::filesystem::path root_path;                 ///< Extraction path.
    std::filesystem::path original_path;             ///< File path.
};
