#pragma once

#include "checker.h"
#include <filesystem>
#include <libxml/tree.h>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

class DirectoryCheckerBase : public Checker
{
  public:
    void validate(const std::filesystem::path& path, Certificate& cert) override;

  protected:
    virtual void performVersionSpecificChecks(const std::filesystem::path& path, Certificate& cert,
                                              const std::map<std::string, std::string>& model_identifiers,
                                              const std::set<std::string>& listed_sources_in_md) = 0;

    static std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);
};
