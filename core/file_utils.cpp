#include "file_utils.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace file_utils
{
static bool isValidUtf8(const std::vector<unsigned char>& data)
{
    size_t i = 0;
    while (i < data.size())
    {
        if (data[i] <= 0x7F)
        {
            i += 1;
        }
        else if ((data[i] & 0xE0) == 0xC0)
        {
            if (i + 1 >= data.size() || (data[i + 1] & 0xC0) != 0x80)
                return false;
            i += 2;
        }
        else if ((data[i] & 0xF0) == 0xE0)
        {
            if (i + 2 >= data.size() || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80)
                return false;
            i += 3;
        }
        else if ((data[i] & 0xF8) == 0xF0)
        {
            if (i + 3 >= data.size() || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80 ||
                (data[i + 3] & 0xC0) != 0x80)
                return false;
            i += 4;
        }
        else
        {
            return false;
        }
    }
    return true;
}

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
    if (std::any_of(buffer.begin(), buffer.end(), [](unsigned char c) { return c == '\0'; }))
        return true;

    // Check for UTF-8 validity
    return !isValidUtf8(buffer);
}

static std::string escapeJson(const std::string& s)
{
    std::ostringstream o;
    for (unsigned char c : s)
    {
        if (c == '"')
            o << "\\\"";
        else if (c == '\\')
            o << "\\\\";
        else if (c < 0x20)
        {
            o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
        }
        else
        {
            o << c;
        }
    }
    return o.str();
}

static void getFileTreeRecursive(const std::filesystem::path& path, std::ostream& os)
{
    std::string name = path.filename().string();
    const bool is_dir = std::filesystem::is_directory(path);
    bool binary = false;

    if (!is_dir)
    {
        binary = isBinary(path);
        if (binary)
            name += " (binary)";
    }

    os << "{";
    os << "\"name\":\"" << escapeJson(name) << "\",";
    os << "\"path\":\"" << escapeJson(path.string()) << "\",";
    os << "\"kind\":\"" << (is_dir ? "directory" : "file") << "\",";
    os << "\"isBinary\":" << (binary ? "true" : "false");

    if (is_dir)
    {
        os << ",\"children\":[";
        std::vector<std::filesystem::path> entries;
        for (const auto& entry : std::filesystem::directory_iterator(path))
            entries.push_back(entry.path());

        std::sort(entries.begin(), entries.end(),
                  [](const auto& a, const auto& b)
                  {
                      bool a_is_dir = std::filesystem::is_directory(a);
                      bool b_is_dir = std::filesystem::is_directory(b);
                      if (a_is_dir != b_is_dir)
                          return a_is_dir;
                      return a.filename().string() < b.filename().string();
                  });

        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (i > 0)
                os << ",";
            getFileTreeRecursive(entries[i], os);
        }
        os << "]";
    }

    os << "}";
}

std::string getFileTreeJson(const std::filesystem::path& root)
{
    if (!std::filesystem::exists(root))
        return "{}";

    std::ostringstream os;
    getFileTreeRecursive(root, os);
    return os.str();
}
} // namespace file_utils
