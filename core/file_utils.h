#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <utility>

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
 * Returns a std::filesystem::path from a UTF-8 encoded string.
 * On Windows, this handles potential non-ASCII characters correctly.
 */
std::filesystem::path utf8ToPath(const std::string& utf8);

/**
 * Generates a JSON representation of the file tree starting at root.
 */
std::string getFileTreeJson(const std::filesystem::path& root);

/**
 * Returns the dimensions (width, height) of a PNG file.
 */
std::optional<std::pair<uint32_t, uint32_t>> getPngDimensions(const std::filesystem::path& path);

/**
 * Low-level helper to serialize a file node to a RapidJSON Value.
 * node and allocator are expected to be rapidjson::Value* and rapidjson::Document::AllocatorType*.
 */
void fileNodeToJson(const std::filesystem::path& path, void* node, void* allocator);
} // namespace file_utils
