#pragma once

#include "checker.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct XmlFileRule
{
    std::filesystem::path relative_path;
    std::string schema_filename;
    bool is_mandatory;
    std::string validation_name;
};

class SchemaCheckerBase : public Checker
{
  public:
    void validate(const std::filesystem::path& path, Certificate& cert) override;

    // Static helper for version extraction (used by factory)
    static std::optional<std::string> extractVersionFromXml(const std::filesystem::path& xml_path,
                                                            const std::string& root_element,
                                                            const std::string& version_attribute);

  protected:
    // Each derived class implements these
    virtual std::vector<XmlFileRule> getXmlRules(const std::filesystem::path& path) const = 0;
    virtual std::string getStandardName() const = 0;
    virtual std::string getStandardVersion() const = 0;

    // Common utility methods
    std::filesystem::path findSchemaPath(const std::string& schema_filename) const;

    void validateXmlFile(const std::filesystem::path& xml_path, const std::filesystem::path& schema_path,
                         const std::string& validation_name, Certificate& cert);

    // UTF-8 encoding validation methods
    bool validateUtf8Encoding(const std::filesystem::path& xml_path, const std::string& validation_name,
                              Certificate& cert);

    static bool isValidUtf8File(const std::filesystem::path& file_path);
    static bool isValidUtf8(const unsigned char* data, size_t length);

  private:
    static void errorCallback(void* ctx, const char* msg, ...);
    static void warningCallback(void* ctx, const char* msg, ...);
};