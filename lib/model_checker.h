#pragma once

#include "certificate.h"
#include "checker.h"

#include <filesystem>

class ModelChecker
{
  public:
    // Main validation method
    void validate(const std::filesystem::path& path) const;

    // Internal validation method for recursive/quiet checking
    Certificate validateCore(const std::filesystem::path& path) const;

  private:
    void validateInternal(const std::filesystem::path& path, Certificate& cert) const;

  public:
    // Certificate management
    bool addCertificate(const std::filesystem::path& path) const;
    bool updateCertificate(const std::filesystem::path& path) const;
    bool removeCertificate(const std::filesystem::path& path) const;
    bool displayCertificate(const std::filesystem::path& path) const;

  private:
    // Helper functions for FMU manipulation
    bool extract(const std::filesystem::path& fmu_path, const std::filesystem::path& extract_dir) const;
    bool package(const std::filesystem::path& extract_dir, const std::filesystem::path& fmu_path) const;

    // Utility functions
    std::string calculateSHA256(const std::filesystem::path& path) const;
};