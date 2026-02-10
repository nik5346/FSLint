#pragma once

#include "binary_checker_base.h"

class Fmi3BinaryChecker : public BinaryCheckerBase
{
  protected:
    std::vector<std::string> getExpectedFunctions() override;
};
