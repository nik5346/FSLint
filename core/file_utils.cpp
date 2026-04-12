#include "file_utils.h"

#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
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

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize bytes_read = file.gcount();
    buffer.resize(static_cast<size_t>(bytes_read));

    if (bytes_read == 0)
        return false;

    // Check for null bytes
    return std::ranges::any_of(buffer, [](unsigned char c) { return c == '\0'; });
}

std::string pathToUtf8(const std::filesystem::path& path)
{
#ifdef _WIN32
    const std::u8string u8_path = path.u8string();

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return {reinterpret_cast<const char*>(u8_path.data()), u8_path.size()};
#else
    return path.string();
#endif
}

std::filesystem::path utf8ToPath(const std::string& utf8)
{
#ifdef _WIN32
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size());
#else
    return {utf8};
#endif
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void fileNodeToJson(const std::filesystem::path& path, void* node_ptr, void* allocator_ptr)
{

    auto& node =
        *reinterpret_cast /* NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) */ /* NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                                                                     */
        /* NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) */ /* NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                                                   */
        /* NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) */<rapidjson::Value*>(node_ptr);

    auto& allocator =
        *reinterpret_cast /* NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) */ /* NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                                                                     */
        /* NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) */ /* NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                                                   */
        /* NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) */<rapidjson::Document::AllocatorType*>(allocator_ptr);

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

        std::ranges::sort(entries,
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

bool hasPngMagic(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;

    std::array<unsigned char, 8> header{};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    file.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (file.gcount() < 8)
        return false;

    // PNG signature: 89 50 4E 47 0D 0A 1A 0A
    static constexpr std::array<unsigned char, 8> signature = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    return std::memcmp(header.data(), signature.data(), signature.size()) == 0;
}

std::optional<std::pair<uint32_t, uint32_t>> getPngDimensions(const std::filesystem::path& path)
{
    if (!hasPngMagic(path))
        return std::nullopt;

    std::ifstream file(path, std::ios::binary);
    if (!file)
        return std::nullopt;

    // PNG header (8 bytes) + IHDR chunk header (4 bytes length + 4 bytes type + 8 bytes width/height)
    std::array<unsigned char, 24> header{};

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    file.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (file.gcount() < 24)
        return std::nullopt;

    // Check IHDR chunk type: "IHDR"
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (std::memcmp(header.data() + 12, "IHDR", 4) != 0)
        return std::nullopt;

    const auto readUint32Be = [](const unsigned char* data) -> uint32_t
    {
        return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
               (static_cast<uint32_t>(data[2]) << 8) | (static_cast<uint32_t>(data[3]));
    };

    const uint32_t width = readUint32Be(header.data() + 16);
    const uint32_t height = readUint32Be(header.data() + 20);
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    return std::make_pair(width, height);
}

uint64_t getTotalSize(const std::filesystem::path& path)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
        return 0;

    if (std::filesystem::is_regular_file(path, ec))
        return std::filesystem::file_size(path, ec);

    if (std::filesystem::is_directory(path, ec))
    {
        uint64_t total = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path, ec))
        {
            if (ec)
                break;
            if (entry.is_regular_file(ec))
                total += entry.file_size(ec);
        }
        return total;
    }

    return 0;
}
} // namespace file_utils
