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
#include <system_error>
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

std::string pathToUtf8(const std::filesystem::path& path)
{
#ifdef _WIN32
    const std::u8string u8_path = path.u8string();
    return std::string(reinterpret_cast<const char*>(u8_path.data()), u8_path.size());
#else
    return path.string();
#endif
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void fileNodeToJson(const std::filesystem::path& path, void* node_ptr, void* allocator_ptr)
{
    auto& node = *reinterpret_cast<rapidjson::Value*>(node_ptr);
    auto& allocator = *reinterpret_cast<rapidjson::Document::AllocatorType*>(allocator_ptr);

    std::error_code ec;
    const std::string name = pathToUtf8(path.filename());
    const bool is_dir = std::filesystem::is_directory(path, ec);
    if (ec)
        return;

    const bool binary = !is_dir && isBinary(path);

    node.SetObject();
    node.AddMember("name", rapidjson::Value(name.c_str(), allocator).Move(), allocator);
    node.AddMember("path", rapidjson::Value(pathToUtf8(path).c_str(), allocator).Move(), allocator);
    node.AddMember("kind", rapidjson::Value(is_dir ? "directory" : "file", allocator).Move(), allocator);
    node.AddMember("isBinary", binary, allocator);

    if (!is_dir)
    {
        const auto size = std::filesystem::file_size(path, ec);
        node.AddMember("size", static_cast<uint64_t>(ec ? 0 : size), allocator);
    }

    if (is_dir)
    {
        rapidjson::Value children(rapidjson::kArrayType);
        std::vector<std::filesystem::path> entries;
        for (const auto& entry : std::filesystem::directory_iterator(path, ec))
            if (!ec)
                entries.push_back(entry.path());

        std::sort(entries.begin(), entries.end(),
                  [](const auto& a, const auto& b)
                  {
                      std::error_code ec_a;
                      std::error_code ec_b;
                      const bool a_is_dir = std::filesystem::is_directory(a, ec_a);
                      const bool b_is_dir = std::filesystem::is_directory(b, ec_b);
                      if (a_is_dir != b_is_dir)
                          return a_is_dir;
                      return pathToUtf8(a.filename()) < pathToUtf8(b.filename());
                  });

        for (const auto& entry : entries)
        {
            rapidjson::Value child;
            fileNodeToJson(entry, &child, &allocator);
            if (child.IsObject())
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
