#pragma once

#include <filesystem>

namespace file_utils
{
/**
 * Checks if a file is binary by looking for null bytes in the first 1KB.
 */
bool isBinary(const std::filesystem::path& path);

/**
 * Returns a UTF-8 encoded string representing the path.
 * On Windows, this handles potential non-ASCII characters correctly.
 */
std::string pathToUtf8(const std::filesystem::path& path);

/**
 * Generates a JSON representation of the file tree starting at root.
 */
std::string getFileTreeJson(const std::filesystem::path& root);

/**
 * Low-level helper to serialize a file node to a RapidJSON Value.
 * node and allocator are expected to be rapidjson::Value* and rapidjson::Document::AllocatorType*.
 */
void fileNodeToJson(const std::filesystem::path& path, void* node, void* allocator);
} // namespace file_utils
