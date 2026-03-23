#pragma once

#include <filesystem>
#include <string>

namespace file_utils
{
/**
 * Safely converts a path to a UTF-8 encoded std::string.
 */
std::string pathToUtf8(const std::filesystem::path& path);

/**
 * Safely converts a UTF-8 encoded std::string back to a std::filesystem::path.
 */
std::filesystem::path utf8ToPath(const std::string& utf8);

/**
 * Checks if a file is binary by looking for null bytes in the first 1KB.
 */
bool isBinary(const std::filesystem::path& path);

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
