#pragma once

#include "checker.h"
#include "model_info.h"

#include <filesystem>
#include <memory>
#include <vector>

/// @brief Factory for detecting standards and creating appropriate checkers.
class CheckerFactory
{
  public:
    /// @brief Detects standard and version of a model.
    /// @param extract_dir Extraction directory.
    /// @param original_path Original file path.
    /// @return Detected ModelInfo.
    static ModelInfo detectModel(const std::filesystem::path& extract_dir,
                                 const std::filesystem::path& original_path = "");

    /// @brief Creates the suite of checkers for a detected model.
    /// @param info Detected ModelInfo.
    /// @return List of unique pointers to checkers.
    static std::vector<std::unique_ptr<Checker>> createCheckers(const ModelInfo& info);

  private:
    static std::unique_ptr<Checker> createSchemaChecker(const ModelInfo& info);
    static std::unique_ptr<Checker> createModelDescriptionChecker(const ModelInfo& info);
    static std::unique_ptr<Checker> createTerminalsAndIconsChecker(const ModelInfo& info);
    static std::unique_ptr<Checker> createBuildDescriptionChecker(const ModelInfo& info);
    static std::unique_ptr<Checker> createDirectoryChecker(const ModelInfo& info);
    static std::unique_ptr<Checker> createBinaryChecker(const ModelInfo& info);
};
