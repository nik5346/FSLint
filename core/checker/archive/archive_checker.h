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

    /// @brief Checks for Zip Slip (path traversal) by resolving canonical paths.
    /// @param entries The list of ZIP file entries.
    /// @param cert The certificate to record results.
    void checkZipSlip(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;

    /// @brief Checks for Zip Bombs (decompression bombs) based on size and ratio.
    /// @param entries The list of ZIP file entries.
    /// @param cert The certificate to record results.
    void checkZipBomb(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;

    /// @brief Checks for duplicate entry names and case-conflicts.
    /// @param entries The list of ZIP file entries.
    /// @param cert The certificate to record results.
    void checkDuplicateNames(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;

    /// @brief Checks for overlapping file data regions.
    /// @param entries The list of ZIP file entries.
    /// @param cert The certificate to record results.
    void checkOverlappingEntries(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;

    /// @brief Checks for consistency between central directory and local headers.
    /// @param path The path to the ZIP archive.
    /// @param entries The list of ZIP file entries.
    /// @param cert The certificate to record results.
    void checkCentralDirectoryConsistency(const std::filesystem::path& path, const std::vector<ZipFileEntry>& entries,
                                          Certificate& cert) const;

    /// @brief Checks for entry count sanity (reported vs actual).
    /// @param handler The ZIP handler.
    /// @param entries The list of ZIP file entries.
    /// @param cert The certificate to record results.
    void checkEntryCountSanity(const Zipper& handler, const std::vector<ZipFileEntry>& entries,
                               Certificate& cert) const;

    /// @brief Checks for extra field and comment integrity.
    /// @param handler The ZIP handler.
    /// @param entries The list of ZIP file entries.
    /// @param cert The certificate to record results.
    void checkExtraFieldsAndComments(const Zipper& handler, const std::vector<ZipFileEntry>& entries,
                                     Certificate& cert) const;

  private:
    void checkFileExtension(const std::filesystem::path& path, Certificate& cert) const;
    void checkCompressionMethods(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkVersionNeeded(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkEncryption(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkSymbolicLinks(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkGeneralPurposeBit3(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkDiskSpanning(Zipper& handler, Certificate& cert) const;
};
