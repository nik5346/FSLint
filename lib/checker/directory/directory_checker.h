#pragma once

#include "checker.h"
#include <filesystem>
#include <libxml/tree.h>
#include <map>
#include <optional>
#include <string>
#include <vector>

class DirectoryChecker : public Checker
{
  public:
    void validate(const std::filesystem::path& path, Certificate& cert) override;

  private:
    std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);
};
