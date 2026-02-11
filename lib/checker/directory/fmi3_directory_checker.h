#pragma once

#include "directory_checker.h"

class Fmi3DirectoryChecker : public DirectoryChecker
{
  protected:
    void performVersionSpecificChecks(const std::filesystem::path& path, Certificate& cert,
                                      const std::map<std::string, std::string>& model_identifiers,
                                      const std::set<std::string>& listed_sources_in_md,
                                      bool needs_execution_tool) override;
};
