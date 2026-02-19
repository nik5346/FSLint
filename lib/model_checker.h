#pragma once

#include "certificate.h"
#include "checker.h"

#include <filesystem>
#include <string>

class ModelChecker
{
  public:
    // Main validation method
    Certificate validate(const std::filesystem::path& path, bool quiet = false) const;

  public:
    // Certificate management
    bool addCertificate(const std::filesystem::path& path) const;
    bool updateCertificate(const std::filesystem::path& path) const;
    bool removeCertificate(const std::filesystem::path& path) const;
    bool displayCertificate(const std::filesystem::path& path) const;
    bool verifyCertificate(const std::filesystem::path& path) const;

    // Version management
    bool isVersionDeprecated(const std::string& version) const;

  private:
    // Helper functions for FMU manipulation
    bool extract(const std::filesystem::path& fmu_path, const std::filesystem::path& extract_dir) const;
    bool package(const std::filesystem::path& extract_dir, const std::filesystem::path& fmu_path) const;

    // Utility functions
    std::string calculateSHA256(const std::filesystem::path& path) const;
};