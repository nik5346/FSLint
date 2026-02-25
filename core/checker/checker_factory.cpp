#include "checker_factory.h"

#include "model_info.h"

#include "checker.h"

#include "fmi1_schema_checker.h"
#include "fmi2_schema_checker.h"
#include "fmi3_schema_checker.h"
#include "schema_checker.h"
#include "ssp1_schema_checker.h"
#include "ssp2_schema_checker.h"

#include "fmi1_model_description_checker.h"
#include "fmi2_model_description_checker.h"
#include "fmi3_model_description_checker.h"

#include "fmi2_terminals_and_icons_checker.h"
#include "fmi3_terminals_and_icons_checker.h"

#include "fmi2_build_description_checker.h"
#include "fmi3_build_description_checker.h"

#include "fmi1_binary_checker.h"
#include "fmi2_binary_checker.h"
#include "fmi3_binary_checker.h"

#include "fmi1_directory_checker.h"
#include "fmi2_directory_checker.h"
#include "fmi3_directory_checker.h"

#include "resources_checker.h"

#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

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

            // Determine FMI1, FMI2 vs FMI3 based on version
            if (version->starts_with("1.0"))
                if (SchemaCheckerBase::hasElement(model_desc_path, "Implementation"))
                    info.standard = ModelStandard::FMI1_CS;
                else
                    info.standard = ModelStandard::FMI1_ME;
            else if (version->starts_with("2.0"))
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
        if (version)
        {
            if (version->starts_with("1.0"))
            {
                info.version = *version;
                info.standard = ModelStandard::SSP1;
            }
            else if (version->starts_with("2.0"))
            {
                info.version = *version;
                info.standard = ModelStandard::SSP2;
            }
        }

        return info;
    }

    return info;
}

std::vector<std::unique_ptr<Checker>> CheckerFactory::createCheckers(const ModelInfo& info)
{
    std::vector<std::unique_ptr<Checker>> checkers;

    if (auto checker = createSchemaChecker(info))
        checkers.push_back(std::move(checker));

    if (auto checker = createModelDescriptionChecker(info))
        checkers.push_back(std::move(checker));

    if (auto checker = createTerminalsAndIconsChecker(info))
        checkers.push_back(std::move(checker));

    if (auto checker = createBuildDescriptionChecker(info))
        checkers.push_back(std::move(checker));

    if (auto checker = createDirectoryChecker(info))
        checkers.push_back(std::move(checker));

    if (auto checker = createBinaryChecker(info))
        checkers.push_back(std::move(checker));

    checkers.push_back(std::make_unique<ResourcesChecker>());

    return checkers;
}

std::unique_ptr<Checker> CheckerFactory::createSchemaChecker(const ModelInfo& info)
{
    switch (info.standard)
    {
    case ModelStandard::FMI1_ME:
        return std::make_unique<Fmi1MeSchemaChecker>();
    case ModelStandard::FMI1_CS:
        return std::make_unique<Fmi1CsSchemaChecker>();
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

std::unique_ptr<Checker> CheckerFactory::createModelDescriptionChecker(const ModelInfo& info)
{
    std::unique_ptr<ModelDescriptionCheckerBase> checker;
    switch (info.standard)
    {
    case ModelStandard::FMI1_ME:
        [[fallthrough]];
    case ModelStandard::FMI1_CS:
        checker = std::make_unique<Fmi1ModelDescriptionChecker>();
        break;
    case ModelStandard::FMI2:
        checker = std::make_unique<Fmi2ModelDescriptionChecker>();
        break;
    case ModelStandard::FMI3:
        checker = std::make_unique<Fmi3ModelDescriptionChecker>();
        break;
    case ModelStandard::SSP1:
        [[fallthrough]];
    case ModelStandard::SSP2:
        [[fallthrough]];
    default:
        return nullptr;
    }

    if (checker)
        checker->setOriginalPath(info.original_path);

    return checker;
}

std::unique_ptr<Checker> CheckerFactory::createTerminalsAndIconsChecker(const ModelInfo& info)
{
    switch (info.standard)
    {
    case ModelStandard::FMI1_ME:
        [[fallthrough]];
    case ModelStandard::FMI1_CS:
        return nullptr;
    case ModelStandard::FMI2:
        return std::make_unique<Fmi2TerminalsAndIconsChecker>();
    case ModelStandard::FMI3:
        return std::make_unique<Fmi3TerminalsAndIconsChecker>();
    case ModelStandard::SSP1:
        [[fallthrough]];
    case ModelStandard::SSP2:
        [[fallthrough]];
    default:
        return nullptr;
    }
}

std::unique_ptr<Checker> CheckerFactory::createBuildDescriptionChecker(const ModelInfo& info)
{
    switch (info.standard)
    {
    case ModelStandard::FMI1_ME:
        [[fallthrough]];
    case ModelStandard::FMI1_CS:
        return nullptr;
    case ModelStandard::FMI2:
        return std::make_unique<Fmi2BuildDescriptionChecker>(info.version);
    case ModelStandard::FMI3:
        return std::make_unique<Fmi3BuildDescriptionChecker>(info.version);
    case ModelStandard::SSP1:
        [[fallthrough]];
    case ModelStandard::SSP2:
        [[fallthrough]];
    default:
        return nullptr;
    }
}

std::unique_ptr<Checker> CheckerFactory::createDirectoryChecker(const ModelInfo& info)
{
    switch (info.standard)
    {
    case ModelStandard::FMI1_ME:
        [[fallthrough]];
    case ModelStandard::FMI1_CS:
        return std::make_unique<Fmi1DirectoryChecker>(info.original_path);
    case ModelStandard::FMI2:
        return std::make_unique<Fmi2DirectoryChecker>();
    case ModelStandard::FMI3:
        return std::make_unique<Fmi3DirectoryChecker>();
    case ModelStandard::SSP1:
        [[fallthrough]];
    case ModelStandard::SSP2:
        [[fallthrough]];
    default:
        return nullptr;
    }
}

std::unique_ptr<Checker> CheckerFactory::createBinaryChecker(const ModelInfo& info)
{
    switch (info.standard)
    {
    case ModelStandard::FMI1_ME:
        [[fallthrough]];
    case ModelStandard::FMI1_CS:
        return std::make_unique<Fmi1BinaryChecker>();
    case ModelStandard::FMI2:
        return std::make_unique<Fmi2BinaryChecker>();
    case ModelStandard::FMI3:
        return std::make_unique<Fmi3BinaryChecker>();
    case ModelStandard::SSP1:
        [[fallthrough]];
    case ModelStandard::SSP2:
        [[fallthrough]];
    default:
        return nullptr;
    }
}