#pragma once

#include "directory_checker.h"

#include "certificate.h"

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>

class Fmi1DirectoryChecker : public DirectoryChecker
{
  public:
    Fmi1DirectoryChecker(std::filesystem::path original_path = "")
        : m_original_path(std::move(original_path))
    {
    }
    void validate(const std::filesystem::path& path, Certificate& cert) const override;

  protected:
    void performVersionSpecificChecks(const std::filesystem::path& path, Certificate& cert,
                                      const std::map<std::string, std::string>& model_identifiers,
                                      const std::set<std::string>& listed_sources_in_md,
                                      bool needs_execution_tool) const override;

  private:
    std::filesystem::path m_original_path;
};
