#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace file_utils
{
struct FileNode
{
    std::string name;
    std::string path;
    bool is_directory;
    bool is_binary;
    std::vector<FileNode> children;
};

/**
 * Checks if a file is binary by looking for null bytes in the first 1KB.
 */
bool isBinary(const std::filesystem::path& path);

/**
 * Recursively gets the file tree starting at root.
 */
FileNode getFileTree(const std::filesystem::path& root);

/**
 * Generates a JSON representation of the file tree starting at root.
 */
std::string getFileTreeJson(const std::filesystem::path& root);

/**
 * Converts a FileNode structure to a RapidJSON value.
 */
void fileNodeToJson(const FileNode& node, void* value, void* allocator);
} // namespace file_utils
