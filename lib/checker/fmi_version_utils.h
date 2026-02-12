#pragma once
#include <regex>
#include <string>
#include <string_view>

namespace FmiVersionUtils
{
// FMI 3.0 regex from XSD: 3[.](0|[1-9][0-9]*)([.](0|[1-9][0-9]*))?(-.+)?
inline constexpr std::string_view FMI3_VERSION_PATTERN = R"(^3\.(0|[1-9][0-9]*)(\.(0|[1-9][0-9]*))?(-.+)?$)";

inline bool isValidFmi3Version(const std::string& version)
{
    static const std::regex pattern{std::string(FMI3_VERSION_PATTERN)};
    return std::regex_match(version, pattern);
}
} // namespace FmiVersionUtils
