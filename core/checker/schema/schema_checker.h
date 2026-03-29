#pragma once

#include "checker.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/// @brief Rule for validating an XML file against an XSD schema.
struct XmlFileRule
{
    std::filesystem::path relative_path; ///< Path relative to the model root.
    std::string schema_filename;         ///< Name of the XSD schema file.
    bool is_mandatory;                   ///< True if the file must exist.
    std::string validation_name;         ///< User-friendly name for the validation step.
};

/// @brief Base class for XML schema validation using libxml2.
///
/// Handles schema lookup, UTF-8 encoding checks, and version-specific rules.
class SchemaCheckerBase : public Checker
{
  public:
    /// @brief Validates all XML files defined by the derived class rules.
    /// @param path Model root directory.
    /// @param cert Certificate to record results.
    void validate(const std::filesystem::path& path, Certificate& cert) const override;

    /// @brief Extracts a version attribute from an XML file without full schema validation.
    /// @param xml_path Path to the XML file.
    /// @param root_element Name of the root XML element.
    /// @param version_attribute Name of the version attribute to extract.
    /// @return The version string or std::nullopt if not found.
    static std::optional<std::string> extractVersionFromXml(const std::filesystem::path& xml_path,
                                                            const std::string& root_element,
                                                            const std::string& version_attribute);

    /// @brief Checks if a specific element exists in an XML file.
    /// @param xml_path Path to the XML file.
    /// @param element_name Name of the element to find.
    /// @return True if the element exists.
    static bool hasElement(const std::filesystem::path& xml_path, const std::string& element_name);

  protected:
    /// @brief Gets the list of XML file rules to validate.
    /// @param path Model root directory.
    /// @return Vector of XmlFileRule objects.
    virtual std::vector<XmlFileRule> getXmlRules(const std::filesystem::path& path) const = 0;

    /// @brief Gets the standard name (e.g., "fmi" or "ssp").
    /// @return Standard name.
    virtual std::string getStandardName() const = 0;

    /// @brief Gets the standard version (e.g., "3.0").
    /// @return Version string.
    virtual std::string getStandardVersion() const = 0;

    /// @brief Hook to specify if UTF-8 is strictly required for XML files.
    /// @return True if UTF-8 is mandatory.
    virtual bool isUtf8Required() const
    {
        return true;
    }

    /// @brief Finds the full path to an XSD schema file.
    /// @param schema_filename Filename of the schema.
    /// @param version_override Optional version directory to look in.
    /// @return The filesystem path to the schema.
    std::filesystem::path findSchemaPath(const std::string& schema_filename,
                                         const std::string& version_override = "") const;

    /// @brief Validates a specific XML file against a schema.
    /// @param xml_path Path to the XML file.
    /// @param schema_path Path to the XSD schema.
    /// @param validation_name User-friendly name for the test result.
    /// @param cert Certificate to record results.
    void validateXmlFile(const std::filesystem::path& xml_path, const std::filesystem::path& schema_path,
                         const std::string& validation_name, Certificate& cert) const;

    /// @brief Validates the encoding of an XML file.
    /// @param xml_path Path to the XML file.
    /// @param validation_name User-friendly name for the test result.
    /// @param cert Certificate to record results.
    /// @return True if encoding is valid.
    bool validateUtf8Encoding(const std::filesystem::path& xml_path, const std::string& validation_name,
                              Certificate& cert) const;

    /// @brief Checks if a file contains valid UTF-8 data.
    /// @param file_path Path to the file.
    /// @return True if valid UTF-8.
    static bool isValidUtf8File(const std::filesystem::path& file_path);

    /// @brief Checks if a buffer contains valid UTF-8 data.
    /// @param data Pointer to the data.
    /// @param length Length of the buffer.
    /// @return True if valid UTF-8.
    static bool isValidUtf8(const unsigned char* data, size_t length);

  private:
    static void errorCallback(void* ctx, const char* msg, ...);
    static void warningCallback(void* ctx, const char* msg, ...);
};
