#pragma once

#include <filesystem>
#include <string>

enum class ModelStandard
{
    FMI1_ME,
    FMI1_CS,
    FMI2,
    FMI3,
    SSP1,
    SSP2,
    UNKNOWN
};

struct ModelInfo
{
    ModelStandard standard = ModelStandard::UNKNOWN;
    std::string version;
    std::filesystem::path root_path;
};