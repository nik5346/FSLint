#pragma once

#include "binary_checker.h"

class Fmi2BinaryChecker : public BinaryChecker
{
  protected:
    std::vector<std::string> getExpectedFunctions() override;
};
