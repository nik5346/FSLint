#pragma once

#include "build_description_checker.h"

#include "certificate.h"

#include <libxml/tree.h>

#include <string>
#include <utility>

/// @brief Validator for FMI 3.0 buildDescription.xml.
class Fmi3BuildDescriptionChecker : public BuildDescriptionChecker
{
  public:
    /// @brief Constructor.
    /// @param fmi_version Version.
    explicit Fmi3BuildDescriptionChecker(std::string fmi_version)
        : BuildDescriptionChecker(std::move(fmi_version))
    {
    }

  protected:
    /// @brief Validates version.
    /// @param root XML node.
    /// @param cert Certificate to record results.
    void checkFmiVersion(xmlNodePtr root, Certificate& cert) const override;
};
