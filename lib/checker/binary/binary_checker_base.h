#pragma once

#include "checker.h"
#include <libxml/tree.h>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

class BinaryCheckerBase : public Checker
{
  public:
    void validate(const std::filesystem::path& path, Certificate& cert) override;

  protected:
    virtual std::vector<std::string> getExpectedFunctions() = 0;
    static std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);
};
