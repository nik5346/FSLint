#pragma once

#include "certificate.h"
#include "checker.h"

#include <filesystem>
#include <string>

#include <string_view>

/// @brief Orchestrator for FMU/SSP validation and certificate management.
class ModelChecker
{
  public:
    /// @brief Relative path to the certificate file within the model.
    static constexpr std::string_view CERT_RELATIVE_PATH = "extra/org.fslint/cert.txt";

    /// @brief Virtual destructor for base class with virtual methods.
    virtual ~ModelChecker() = default;

    /// @brief Default constructor.
    ModelChecker() = default;

    /// @brief Copy constructor.
    /// @param other Instance to copy from.
    ModelChecker(const ModelChecker& other) = default;

    /// @brief Move constructor.
    /// @param other Instance to move from.
    ModelChecker(ModelChecker&& other) noexcept = default;

    /// @brief Copy assignment operator.
    /// @param other Instance to copy from.
    /// @return Reference to this instance.
    ModelChecker& operator=(const ModelChecker& other) = default;

    /// @brief Move assignment operator.
    /// @param other Instance to move from.
    /// @return Reference to this instance.
    ModelChecker& operator=(ModelChecker&& other) noexcept = default;

    /// @brief Check if this checker can handle a specific model.
    /// @param path Path to the model.
    /// @return True if handleable.
    [[nodiscard]] virtual bool canHandle(const std::filesystem::path& path) const
    {
        (void)path;
        return true;
    }

    /// @brief Validates a model.
    /// @param path Model path.
    /// @param quiet Suppress logging.
    /// @param show_tree Show file tree.
    /// @param cert Optional certificate to use for results.
    /// @return Certificate.
    [[nodiscard]] virtual Certificate validate(const std::filesystem::path& path, bool quiet = false,
                                               bool show_tree = false, Certificate cert = Certificate()) const;

  public:
    /// @brief Adds certificate to model.
    /// @param path Model path.
    /// @param callback Optional callback for security issues.
    /// @return True if successful.
    [[nodiscard]] bool addCertificate(const std::filesystem::path& path, ContinueCallback callback = nullptr) const;

    /// @brief Updates certificate.
    /// @param path Model path.
    /// @param callback Optional callback for security issues.
    /// @return True if successful.
    [[nodiscard]] bool updateCertificate(const std::filesystem::path& path, ContinueCallback callback = nullptr) const;

    /// @brief Removes certificate.
    /// @param path Model path.
    /// @return True if successful.
    [[nodiscard]] bool removeCertificate(const std::filesystem::path& path) const;

    /// @brief Displays certificate.
    /// @param path Model path.
    /// @return True if successful.
    [[nodiscard]] bool displayCertificate(const std::filesystem::path& path) const;

    /// @brief Verifies certificate.
    /// @param path Model path.
    /// @return True if successful.
    [[nodiscard]] bool verifyCertificate(const std::filesystem::path& path) const;

    /// @brief Checks if version is deprecated.
    /// @param version Version string.
    /// @return True if deprecated.
    [[nodiscard]] bool isVersionDeprecated(const std::string& version) const;

  public:
    /// @brief Extracts model.
    /// @param fmu_path Archive path.
    /// @param extract_dir Extraction directory.
    /// @return True if successful.
    [[nodiscard]] bool extract(const std::filesystem::path& fmu_path, const std::filesystem::path& extract_dir) const;

    /// @brief Packages model.
    /// @param extract_dir Extraction directory.
    /// @param fmu_path Archive path.
    /// @return True if successful.
    [[nodiscard]] bool package(const std::filesystem::path& extract_dir, const std::filesystem::path& fmu_path) const;

  private:
    // Utility functions
    [[nodiscard]] std::string calculateSHA256(const std::filesystem::path& path) const;
};
