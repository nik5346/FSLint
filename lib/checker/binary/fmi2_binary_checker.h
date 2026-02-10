#pragma once

#include "binary_checker_base.h"

class Fmi2BinaryChecker : public BinaryCheckerBase
{
  protected:
    std::vector<std::string> getExpectedFunctions() override;
};
