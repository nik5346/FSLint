#include "file_utils.h"

#include <algorithm>
#include <array>
#include <fstream>

namespace file_utils
{
bool isBinary(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;

    std::array<char, 1024> buffer{};
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize bytes_read = file.gcount();

    return std::any_of(buffer.begin(), buffer.begin() + bytes_read, [](char c) { return c == '\0'; });
}
} // namespace file_utils
