#pragma once

#include "directory_checker_base.h"

class Fmi2DirectoryChecker : public DirectoryCheckerBase
{
  protected:
    void performVersionSpecificChecks(const std::filesystem::path& path, Certificate& cert,
                                      const std::map<std::string, std::string>& model_identifiers,
                                      const std::set<std::string>& listed_sources_in_md) override;
};
