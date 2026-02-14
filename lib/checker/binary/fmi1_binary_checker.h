#pragma once

#include "binary_checker.h"

class Fmi1BinaryChecker : public BinaryChecker
{
  public:
    void validate(const std::filesystem::path& path, Certificate& cert) override;

  protected:
    std::vector<std::string> getExpectedFunctions() override
    {
        return {}; // Not used because we override validate
    }
};
