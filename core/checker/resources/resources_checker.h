#pragma once

#include "checker.h"

#include <filesystem>

class ResourcesChecker : public Checker
{
  public:
    void validate(const std::filesystem::path& path, Certificate& cert) override;

  private:
    void scanResources(const std::filesystem::path& resources_dir, Certificate& cert);
};
