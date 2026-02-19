#pragma once

#include "checker.h"
#include "zipper.h"

#include <filesystem>
#include <vector>

class ArchiveChecker : public Checker
{
  public:
    void validate(const std::filesystem::path& fmu_path, Certificate& cert) override;

  private:
    void checkFileExtension(const std::filesystem::path& path, Certificate& cert);
    void checkCompressionMethods(const std::vector<ZipFileEntry>& entries, Certificate& cert);
    void checkVersionNeeded(const std::vector<ZipFileEntry>& entries, Certificate& cert);
    void checkLanguageEncodingFlag(const std::vector<ZipFileEntry>& entries, Certificate& cert);
    void checkEncryption(const std::vector<ZipFileEntry>& entries, Certificate& cert);
    void checkPathFormat(const std::vector<ZipFileEntry>& entries, Certificate& cert);
    void checkSymbolicLinks(const std::vector<ZipFileEntry>& entries, Certificate& cert);
    void checkGeneralPurposeBit3(const std::vector<ZipFileEntry>& entries, Certificate& cert);
    void checkDiskSpanning(Zipper& handler, Certificate& cert);
};