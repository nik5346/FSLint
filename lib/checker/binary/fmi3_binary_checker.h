#pragma once

#include "binary_checker.h"

class Fmi3BinaryChecker : public BinaryChecker
{
  protected:
    std::vector<std::string> getExpectedFunctions() override;
};
