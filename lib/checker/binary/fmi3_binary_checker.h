#pragma once

#include "binary_checker.h"

#include <string>
#include <vector>

class Fmi3BinaryChecker : public BinaryChecker
{
  protected:
    std::vector<std::string> getExpectedFunctions() override;
};
