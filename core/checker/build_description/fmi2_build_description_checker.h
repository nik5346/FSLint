#pragma once

#include "build_description_checker.h"

#include "certificate.h"

#include <libxml/tree.h>

#include <string>
#include <utility>

/// @brief Validator for FMI 2.0 buildDescription.xml.
class Fmi2BuildDescriptionChecker : public BuildDescriptionChecker
{
  public:
    /// @brief Constructor.
    /// @param fmi_version FMI version.
    explicit Fmi2BuildDescriptionChecker(std::string fmi_version)
        : BuildDescriptionChecker(std::move(fmi_version))
    {
    }

  protected:
    /// @brief Validates version attribute.
    /// @param root XML node.
    /// @param cert Certificate to record results.
    void checkFmiVersion(xmlNodePtr root, Certificate& cert) const override;
};
