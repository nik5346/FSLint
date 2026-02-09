#pragma once

#include "checker.h"
#include <filesystem>
#include <libxml/tree.h>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

class DirectoryChecker : public Checker
{
  public:
    explicit DirectoryChecker(std::string fmi_version) : _fmi_version(std::move(fmi_version))
    {
    }
    void validate(const std::filesystem::path& path, Certificate& cert) override;

  private:
    std::string _fmi_version;
    std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);

    void validateFmi2Structure(const std::filesystem::path& path, Certificate& cert,
                               const std::map<std::string, std::string>& model_identifiers,
                               const std::set<std::string>& listed_sources_in_md);
    void validateFmi3Structure(const std::filesystem::path& path, Certificate& cert,
                               const std::map<std::string, std::string>& model_identifiers);
};
