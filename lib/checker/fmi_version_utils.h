#pragma once
#include <regex>
#include <string>

namespace FmiVersionUtils
{
// FMI 3.0 regex from XSD: 3[.](0|[1-9][0-9]*)([.](0|[1-9][0-9]*))?(-.+)?
inline const std::string FMI3_VERSION_PATTERN = R"(^3\.(0|[1-9][0-9]*)(\.(0|[1-9][0-9]*))?(-.+)?$)";

inline bool isValidFmi3Version(const std::string& version)
{
    static const std::regex pattern(FMI3_VERSION_PATTERN);
    return std::regex_match(version, pattern);
}
} // namespace FmiVersionUtils
