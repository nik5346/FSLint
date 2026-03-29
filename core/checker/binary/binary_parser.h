#pragma once

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

/// @brief Supported binary formats.
enum class BinaryFormat : std::uint8_t
{
    UNKNOWN, ///< Unknown binary format.
    ELF,     ///< Executable and Linkable Format (Linux).
    PE,      ///< Portable Executable (Windows).
    MACHO    ///< Mach-Object (macOS).
};

/// @brief Architecture information for a binary.
struct ArchInfo
{
    int bitness = 0;          ///< Bitness (e.g., 32 or 64).
    std::string architecture; ///< Standardized architecture string.
};

/// @brief Metadata about a binary file.
struct BinaryInfo
{
    BinaryFormat format = BinaryFormat::UNKNOWN; ///< Format of the binary.
    bool isSharedLibrary = false;                ///< True if it is a shared library.
    std::vector<ArchInfo> architectures;         ///< List of architectures (fat binary support).
    std::set<std::string> exports;               ///< Set of exported symbol names.
};

/// @brief Parser for extracting info from binary executables.
class BinaryParser
{
  public:
    /// @brief Parses a binary file (ELF, PE, or Mach-O) to extract information.
    /// @param path The path to the binary file.
    /// @return A BinaryInfo struct containing format, bitness, architecture, and exported symbols.
    static BinaryInfo parse(const std::filesystem::path& path);

    /// @brief Extracts exported C symbols from a binary file (ELF, PE, or Mach-O).
    /// @param path The path to the binary file.
    /// @return A set of exported symbol names.
    static std::set<std::string> getExports(const std::filesystem::path& path);
};
