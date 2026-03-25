#pragma once

#include "checker.h"

#include <filesystem>
#include <string>

class SspDescriptionChecker : public Checker
{
  public:
    void validate(const std::filesystem::path& path, Certificate& cert) const override;
};
