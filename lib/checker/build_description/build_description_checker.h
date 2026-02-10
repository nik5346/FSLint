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

  private:
    std::string _fmi_version;
    std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);

    void checkFmiVersion(xmlNodePtr root, TestResult& test);
    void checkSourceFiles(xmlXPathContextPtr xpath_context, const std::filesystem::path& sources_path, TestResult& test,
                          std::set<std::string>& listed_files);
    void checkIncludeDirectories(xmlXPathContextPtr xpath_context, const std::filesystem::path& sources_path,
                                 TestResult& test);
    void checkBuildConfigurationAttributes(xmlXPathContextPtr xpath_context, const std::set<std::string>& valid_ids,
                                           TestResult& test);

    std::set<std::string> getValidModelIdentifiers(const std::filesystem::path& path);
};
