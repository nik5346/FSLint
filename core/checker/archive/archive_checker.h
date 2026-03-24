#pragma once

#include "checker.h"
#include "zipper.h"

#include <filesystem>
#include <vector>

class ArchiveChecker : public Checker
{
  public:
    void validate(const std::filesystem::path& fmu_path, Certificate& cert) const override;

  private:
    void checkFileExtension(const std::filesystem::path& path, Certificate& cert) const;
    void checkCompressionMethods(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkVersionNeeded(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkLanguageEncodingFlag(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkEncryption(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkPathFormat(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkSymbolicLinks(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkGeneralPurposeBit3(const std::vector<ZipFileEntry>& entries, Certificate& cert) const;
    void checkDiskSpanning(Zipper& handler, Certificate& cert) const;
};