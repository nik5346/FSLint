#pragma once

#include "build_description_checker.h"

#include "certificate.h"

#include <libxml/tree.h>

#include <string>
#include <utility>

class Fmi3BuildDescriptionChecker : public BuildDescriptionChecker
{
  public:
    explicit Fmi3BuildDescriptionChecker(std::string fmi_version)
        : BuildDescriptionChecker(std::move(fmi_version))
    {
    }

  protected:
    void checkFmiVersion(xmlNodePtr root, Certificate& cert) const override;
};
