#pragma once

#include "build_description_checker.h"

#include "certificate.h"

#include <libxml/tree.h>

#include <string>
#include <utility>

class Fmi2BuildDescriptionChecker : public BuildDescriptionChecker
{
  public:
    explicit Fmi2BuildDescriptionChecker(std::string fmi_version)
        : BuildDescriptionChecker(std::move(fmi_version))
    {
    }

  protected:
    void checkFmiVersion(xmlNodePtr root, Certificate& cert) override;
};
