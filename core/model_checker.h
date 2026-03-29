#pragma once

#include "certificate.h"
#include "checker.h"

#include <filesystem>
#include <string>

/// @brief Orchestrator for FMU/SSP validation and certificate management.
class ModelChecker
{
  public:
    /// @brief Validates a model.
    /// @param path Model path.
    /// @param quiet Suppress logging.
    /// @param show_tree Show file tree.
    /// @return Certificate.
    Certificate validate(const std::filesystem::path& path, bool quiet = false, bool show_tree = false) const;

  public:
    /// @brief Adds certificate to model.
    /// @param path Model path.
    /// @return True if successful.
    bool addCertificate(const std::filesystem::path& path) const;

    /// @brief Updates certificate.
    /// @param path Model path.
    /// @return True if successful.
    bool updateCertificate(const std::filesystem::path& path) const;

    /// @brief Removes certificate.
    /// @param path Model path.
    /// @return True if successful.
    bool removeCertificate(const std::filesystem::path& path) const;

    /// @brief Displays certificate.
    /// @param path Model path.
    /// @return True if successful.
    bool displayCertificate(const std::filesystem::path& path) const;

    /// @brief Verifies certificate.
    /// @param path Model path.
    /// @return True if successful.
    bool verifyCertificate(const std::filesystem::path& path) const;

    /// @brief Checks if version is deprecated.
    /// @param version Version string.
    /// @return True if deprecated.
    bool isVersionDeprecated(const std::string& version) const;

  public:
    /// @brief Extracts model.
    /// @param fmu_path Archive path.
    /// @param extract_dir Extraction directory.
    /// @return True if successful.
    bool extract(const std::filesystem::path& fmu_path, const std::filesystem::path& extract_dir) const;

    /// @brief Packages model.
    /// @param extract_dir Extraction directory.
    /// @param fmu_path Archive path.
    /// @return True if successful.
    bool package(const std::filesystem::path& extract_dir, const std::filesystem::path& fmu_path) const;

  private:
    // Utility functions
    std::string calculateSHA256(const std::filesystem::path& path) const;
};
