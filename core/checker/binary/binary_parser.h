#pragma once

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

enum class BinaryFormat : std::uint8_t
{
    UNKNOWN,
    ELF,
    PE,
    MACHO
};

struct ArchInfo
{
    int bitness = 0; // 32 or 64
    std::string architecture;
};

struct BinaryInfo
{
    BinaryFormat format = BinaryFormat::UNKNOWN;
    bool isSharedLibrary = false;
    std::vector<ArchInfo> architectures;
    std::set<std::string> exports;
};

class BinaryParser
{
  public:
    /**
     * @brief Parses a binary file (ELF, PE, or Mach-O) to extract information.
     * @param path The path to the binary file.
     * @return A BinaryInfo struct containing format, bitness, architecture, and exported symbols.
     */
    static BinaryInfo parse(const std::filesystem::path& path);

    /**
     * @brief Extracts exported C symbols from a binary file (ELF, PE, or Mach-O).
     * @param path The path to the binary file.
     * @return A set of exported symbol names.
     */
    static std::set<std::string> getExports(const std::filesystem::path& path);
};
