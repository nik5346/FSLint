#pragma once

#include "checker.h"
#include "model_info.h"

#include <filesystem>
#include <memory>
#include <vector>

class CheckerFactory
{
  public:
    // Detect model type and version from extracted directory
    static ModelInfo detectModel(const std::filesystem::path& extract_dir);

    // Create appropriate checkers for the detected model type
    static std::vector<std::unique_ptr<Checker>> createCheckers(const ModelInfo& info);

  private:
    static std::unique_ptr<Checker> createSchemaChecker(const ModelInfo& info);
    static std::unique_ptr<Checker> createModelDescriptionChecker(const ModelInfo& info);
};