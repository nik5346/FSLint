#pragma once

#include "checker.h"

#include <libxml/tree.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

/// @brief Represents the FMI interface types.
enum class InterfaceType : std::uint8_t
{
    MODEL_EXCHANGE,
    CO_SIMULATION,
    SCHEDULED_EXECUTION
};

/// @brief Base class for validating C symbols in FMI shared libraries.
class BinaryChecker : public Checker
{
  public:
    /// @brief Validates binaries within an extracted FMU directory.
    /// @param path The path to the FMU root directory.
    /// @param cert The certificate to record results.
    void validate(const std::filesystem::path& path, Certificate& cert) const override;

  protected:
    /// @brief Gets the list of mandatory C functions based on supported interfaces.
    /// @param interfaces The set of supported interface types.
    /// @return A vector of function names.
    [[nodiscard]] virtual std::vector<std::string>
    getExpectedFunctions(const std::set<InterfaceType>& interfaces) const = 0;

    /// @brief Extracts a string attribute from an XML node.
    /// @param node XML node.
    /// @param attr_name Name of the attribute.
    /// @return Attribute value or std::nullopt.
    static std::optional<std::string> getXmlAttribute(xmlNodePtr node, const std::string& attr_name);
};
