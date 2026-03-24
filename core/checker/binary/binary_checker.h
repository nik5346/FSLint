#pragma once

#include "checker.h"

#include <libxml/tree.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class BinaryChecker : public Checker
{
  public:
    void validate(const std::filesystem::path& path, Certificate& cert) const override;

  protected:
    virtual std::vector<std::string> getExpectedFunctions() const = 0;
    static std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);
};
