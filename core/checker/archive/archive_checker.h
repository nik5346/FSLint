#pragma once

#include "checker.h"
#include "zipper.h"

#include <filesystem>
#include <vector>

/// @brief Validator for FMU/SSP ZIP archives.
class ArchiveChecker : public Checker
{
  public:
    /// @brief Validates the archive at the given path.
    /// @param fmu_path The path to the ZIP archive.
    /// @param cert The certificate to record results.
    void validate(const std::filesystem::path& fmu_path, Certificate& cert) const override;

  public:
    /// @brief Checks the Language Encoding Flag (Bit 11) for all ZIP entries.
    /// @param entries The list of ZIP file entries.
    /// @param cert The certificate to record results.
    void checkLanguageEncodingFlag(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;

    /// @brief Checks the character format of file paths within the archive.
    /// @param entries The list of ZIP file entries.
    /// @param cert The certificate to record results.
    void checkPathFormat(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;

  private:
    void checkFileExtension(const std::filesystem::path& path, Certificate& cert) const;
    void checkCompressionMethods(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkVersionNeeded(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkEncryption(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkSymbolicLinks(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkGeneralPurposeBit3(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkDiskSpanning(Zipper& handler, Certificate& cert) const;
};
