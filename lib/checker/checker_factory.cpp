#include "checker_factory.h"

#include "fmi2_schema_checker.h"
#include "fmi3_schema_checker.h"
#include "schema_checker.h"
#include "ssp1_schema_checker.h"
#include "ssp2_schema_checker.h"

#include "fmi2_model_description_checker.h"
#include "fmi2_terminals_and_icons_checker.h"
#include "fmi3_model_description_checker.h"
#include "fmi3_terminals_and_icons_checker.h"
#include "model_description_checker.h"
#include "terminals_and_icons_checker.h"

#include "build_description_checker.h"
#include "fmi2_binary_checker.h"
#include "fmi2_build_description_checker.h"
#include "fmi2_directory_checker.h"
#include "fmi3_binary_checker.h"
#include "fmi3_build_description_checker.h"
#include "fmi3_directory_checker.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <utility>

ModelInfo CheckerFactory::detectModel(const std::filesystem::path& extract_dir)
{
    ModelInfo info;
    info.root_path = extract_dir;
    info.standard = ModelStandard::UNKNOWN;

    // Check for FMI
    const auto model_desc_path = extract_dir / "modelDescription.xml";
    if (std::filesystem::exists(model_desc_path))
    {
        // Try to extract FMI version
        auto version = SchemaCheckerBase::extractVersionFromXml(model_desc_path, "fmiModelDescription", "fmiVersion");
        if (version)
        {
            info.version = *version;

            // Determine FMI2 vs FMI3 based on version
            if (version->starts_with("2."))
                info.standard = ModelStandard::FMI2;
            else if (version->starts_with("3."))
                info.standard = ModelStandard::FMI3;
        }
        return info;
    }

    // Check for SSP
    const auto system_structure_path = extract_dir / "SystemStructure.ssd";
    if (std::filesystem::exists(system_structure_path))
    {
        auto version =
            SchemaCheckerBase::extractVersionFromXml(system_structure_path, "SystemStructureDescription", "version");
        if (version->starts_with("1."))
        {
            info.version = *version;
            info.standard = ModelStandard::SSP1;
        }
        else if (version->starts_with("2."))
        {
            info.version = *version;
            info.standard = ModelStandard::SSP2;
        }
        return info;
    }

    return info;
}

std::vector<std::unique_ptr<Checker>> CheckerFactory::createCheckers(const ModelInfo& info)
{
    std::vector<std::unique_ptr<Checker>> checkers;

    // Create schema checker
    if (auto checker = createSchemaChecker(info))
        checkers.push_back(std::move(checker));

    // Create model description checker
    if (auto checker = createModelDescriptionChecker(info))
        checkers.push_back(std::move(checker));

    // Create terminals and icons checker
    if (auto checker = createTerminalsAndIconsChecker(info))
        checkers.push_back(std::move(checker));

    if (info.standard == ModelStandard::FMI2)
    {
        checkers.push_back(std::make_unique<Fmi2DirectoryChecker>());
        checkers.push_back(std::make_unique<Fmi2BuildDescriptionChecker>(info.version));
        checkers.push_back(std::make_unique<Fmi2BinaryChecker>());
    }
    else if (info.standard == ModelStandard::FMI3)
    {
        checkers.push_back(std::make_unique<Fmi3DirectoryChecker>());
        checkers.push_back(std::make_unique<Fmi3BuildDescriptionChecker>(info.version));
        checkers.push_back(std::make_unique<Fmi3BinaryChecker>());
    }

    return checkers;
}

std::unique_ptr<Checker> CheckerFactory::createModelDescriptionChecker(const ModelInfo& info)
{
    switch (info.standard)
    {
    case ModelStandard::FMI2:
        return std::make_unique<Fmi2ModelDescriptionChecker>();
    case ModelStandard::FMI3:
        return std::make_unique<Fmi3ModelDescriptionChecker>();
    case ModelStandard::SSP1:
        [[fallthrough]]; // No model description checker for SSP
    case ModelStandard::SSP2:
        [[fallthrough]]; // No model description checker for SSP
    default:
        return nullptr;
    }
}

std::unique_ptr<Checker> CheckerFactory::createTerminalsAndIconsChecker(const ModelInfo& info)
{
    switch (info.standard)
    {
    case ModelStandard::FMI2:
        return std::make_unique<Fmi2TerminalsAndIconsChecker>();
    case ModelStandard::FMI3:
        return std::make_unique<Fmi3TerminalsAndIconsChecker>();
    default:
        return nullptr;
    }
}

std::unique_ptr<Checker> CheckerFactory::createSchemaChecker(const ModelInfo& info)
{
    switch (info.standard)
    {
    case ModelStandard::FMI2:
        return std::make_unique<Fmi2SchemaChecker>();
    case ModelStandard::FMI3:
        return std::make_unique<Fmi3SchemaChecker>();
    case ModelStandard::SSP1:
        return std::make_unique<Ssp1SchemaChecker>();
    case ModelStandard::SSP2:
        return std::make_unique<Ssp2SchemaChecker>();
    default:
        return nullptr;
    }
}
