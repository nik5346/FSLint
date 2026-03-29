#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <utility>

/// @brief Utility functions for filesystem operations.
namespace file_utils
{
/// @brief Checks if a file is binary.
/// @param path File path.
/// @return True if binary.
bool isBinary(const std::filesystem::path& path);

/// @brief Converts path to UTF-8 string.
/// @param path Path to convert.
/// @return UTF-8 string.
std::string pathToUtf8(const std::filesystem::path& path);

/// @brief Converts UTF-8 string to path.
/// @param utf8 UTF-8 string.
/// @return Path.
std::filesystem::path utf8ToPath(const std::string& utf8);

/// @brief Gets file tree as JSON string.
/// @param root Root directory.
/// @return JSON string.
std::string getFileTreeJson(const std::filesystem::path& root);

/// @brief Gets PNG image dimensions.
/// @param path Path to PNG.
/// @return Pair of width and height or std::nullopt.
std::optional<std::pair<uint32_t, uint32_t>> getPngDimensions(const std::filesystem::path& path);

/// @brief Gets total size of file or directory.
/// @param path Path.
/// @return Size in bytes.
uint64_t getTotalSize(const std::filesystem::path& path);

/// @brief Low-level helper to serialize file node to RapidJSON.
/// @param path File path.
/// @param node RapidJSON Value pointer.
/// @param allocator RapidJSON Allocator pointer.
void fileNodeToJson(const std::filesystem::path& path, void* node, void* allocator);
} // namespace file_utils
