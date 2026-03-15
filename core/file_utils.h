#pragma once

#include <filesystem>

namespace file_utils
{
/**
 * Checks if a file is binary by looking for null bytes in the first 1KB.
 */
bool isBinary(const std::filesystem::path& path);

/**
 * Generates a JSON representation of the file tree starting at root.
 */
std::string getFileTreeJson(const std::filesystem::path& root);
} // namespace file_utils
