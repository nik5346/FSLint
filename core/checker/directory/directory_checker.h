#pragma once

#include "checker.h"

#include <filesystem>
#include <libxml/tree.h>
#include <map>
#include <optional>
#include <set>
#include <string>

class DirectoryChecker : public Checker
{
  public:
    void validate(const std::filesystem::path& path, Certificate& cert) const override;

  protected:
    virtual void performVersionSpecificChecks(const std::filesystem::path& path, Certificate& cert,
                                              const std::map<std::string, std::string>& model_identifiers,
                                              const std::set<std::string>& listed_sources_in_md,
                                              bool needs_execution_tool) const = 0;

    void checkStandardHeaders(const std::filesystem::path& path, Certificate& cert,
                              const std::set<std::string>& headers) const;

    static std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);

    static bool isEffectivelyEmpty(const std::filesystem::path& path);
};
