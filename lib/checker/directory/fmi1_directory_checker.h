#pragma once

#include "directory_checker.h"

class Fmi1DirectoryChecker : public DirectoryChecker
{
  public:
    void validate(const std::filesystem::path& path, Certificate& cert) override;

  protected:
    void performVersionSpecificChecks(const std::filesystem::path& path, Certificate& cert,
                                      const std::map<std::string, std::string>& model_identifiers,
                                      const std::set<std::string>& listed_sources_in_md,
                                      bool needs_execution_tool) override;
};
