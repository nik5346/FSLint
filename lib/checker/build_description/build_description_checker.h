#pragma once

#include "certificate.h"
#include "checker.h"
#include <filesystem>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <optional>
#include <set>
#include <string>

class BuildDescriptionChecker : public Checker
{
  public:
    explicit BuildDescriptionChecker(std::string fmi_version)
        : _fmi_version(std::move(fmi_version))
    {
    }
    void validate(const std::filesystem::path& path, Certificate& cert) override;

  protected:
    std::string _fmi_version;
    std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);

    virtual void checkFmiVersion(xmlNodePtr root, Certificate& cert) = 0;
    void checkSourceFiles(xmlXPathContextPtr xpath_context, const std::filesystem::path& sources_path,
                          Certificate& cert, std::set<std::string>& listed_files);
    void checkIncludeDirectories(xmlXPathContextPtr xpath_context, const std::filesystem::path& sources_path,
                                 Certificate& cert);
    void checkBuildConfigurationAttributes(xmlXPathContextPtr xpath_context, const std::set<std::string>& valid_ids,
                                           Certificate& cert);

    std::set<std::string> getValidModelIdentifiers(const std::filesystem::path& path);
};
