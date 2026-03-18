#include "file_utils.h"

#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

namespace file_utils
{
bool isBinary(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;

    // Read up to 8KB to check for null bytes and UTF-8 validity
    std::vector<unsigned char> buffer(8192);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize bytes_read = file.gcount();
    buffer.resize(static_cast<size_t>(bytes_read));

    if (bytes_read == 0)
        return false;

    // Check for null bytes
    return std::any_of(buffer.begin(), buffer.end(), [](unsigned char c) { return c == '\0'; });
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void fileNodeToJson(const std::filesystem::path& path, void* node_ptr, void* allocator_ptr)
{
    auto& node = *reinterpret_cast<rapidjson::Value*>(node_ptr);
    auto& allocator = *reinterpret_cast<rapidjson::Document::AllocatorType*>(allocator_ptr);

    const std::string name = path.filename().string();
    const bool is_dir = std::filesystem::is_directory(path);
    const bool binary = !is_dir && isBinary(path);

    node.SetObject();
    node.AddMember("name", rapidjson::Value(name.c_str(), allocator).Move(), allocator);
    node.AddMember("path", rapidjson::Value(path.string().c_str(), allocator).Move(), allocator);
    node.AddMember("kind", rapidjson::Value(is_dir ? "directory" : "file", allocator).Move(), allocator);
    node.AddMember("isBinary", binary, allocator);

    if (!is_dir)
        node.AddMember("size", static_cast<uint64_t>(std::filesystem::file_size(path)), allocator);

    if (is_dir)
    {
        rapidjson::Value children(rapidjson::kArrayType);
        std::vector<std::filesystem::path> entries;
        for (const auto& entry : std::filesystem::directory_iterator(path))
            entries.push_back(entry.path());

        std::sort(entries.begin(), entries.end(),
                  [](const auto& a, const auto& b)
                  {
                      const bool a_is_dir = std::filesystem::is_directory(a);
                      const bool b_is_dir = std::filesystem::is_directory(b);
                      if (a_is_dir != b_is_dir)
                          return a_is_dir;
                      return a.filename().string() < b.filename().string();
                  });

        for (const auto& entry : entries)
        {
            rapidjson::Value child;
            fileNodeToJson(entry, &child, &allocator);
            children.PushBack(child, allocator);
        }
        node.AddMember("children", children, allocator);
    }
}

std::string getFileTreeJson(const std::filesystem::path& root)
{
    if (!std::filesystem::exists(root))
        return "{}";

    rapidjson::Document doc;
    doc.SetObject();
    fileNodeToJson(root, &doc, &doc.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    return buffer.GetString();
}
} // namespace file_utils
