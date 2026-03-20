#pragma once

#include "checker.h"

#include <filesystem>

class SspDirectoryChecker : public Checker
{
  public:
    void validate(const std::filesystem::path& path, Certificate& cert) override;
};
