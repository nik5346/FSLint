#pragma once

#include "checker.h"
#include <map>
#include <set>
#include <string>
#include <optional>
#include <libxml/tree.h>

class BinaryChecker : public Checker
{
  public:
    void validate(const std::filesystem::path& path, Certificate& cert) override;

  private:
    std::vector<std::string> getExpectedFunctions(const std::string& version);
    static std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);
};
