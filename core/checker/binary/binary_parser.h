#pragma once

#include <filesystem>
#include <set>
#include <string>

class BinaryParser
{
  public:
    /**
     * @brief Extracts exported C symbols from a binary file (ELF, PE, or Mach-O).
     * @param path The path to the binary file.
     * @return A set of exported symbol names.
     */
    static std::set<std::string> getExports(const std::filesystem::path& path);
};
