#include "zipper.h"

#include "unzip.h"
#include "zip.h"

#include <zconf.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios> // IWYU pragma: keep
#include <iostream>
#include <string>
#include <vector>

Zipper::~Zipper()
{
    close();
}

bool Zipper::open(const std::filesystem::path& zip_path)
{
    close(); // Close any previously opened file

    _zip_path = zip_path;
    _zip_file = unzOpen(zip_path.string().c_str());

    return _zip_file != nullptr;
}

bool Zipper::create(const std::filesystem::path& zip_path)
{
    close(); // Close any previously opened file

    _zip_path = zip_path;
    _zip_writer = zipOpen(zip_path.string().c_str(), APPEND_STATUS_CREATE);

    return _zip_writer != nullptr;
}

void Zipper::close()
{
    if (_zip_file)
    {
        unzClose(static_cast<unzFile>(_zip_file));
        _zip_file = nullptr;
    }

    if (_zip_writer)
    {
        zipClose(static_cast<zipFile>(_zip_writer), nullptr);
        _zip_writer = nullptr;
    }
}

std::vector<ZipFileEntry> Zipper::getEntries() const
{
    std::vector<ZipFileEntry> entries;

    if (!_zip_file)
        return entries;

    unzFile uf = static_cast<unzFile>(_zip_file);

    if (unzGoToFirstFile(uf) != UNZ_OK)
        return entries;

    // Use while loop instead of do-while to avoid the warning
    bool has_files = true;
    while (has_files)
    {
        unz_file_info file_info;
        constexpr size_t MAX_FILENAME_LENGTH = 512;
        std::array<char, MAX_FILENAME_LENGTH> filename{};

        if (unzGetCurrentFileInfo(uf, &file_info, filename.data(), static_cast<uInt>(filename.size()), nullptr, 0,
                                  nullptr, 0) != UNZ_OK)
        {
            has_files = (unzGoToNextFile(uf) == UNZ_OK);
            continue;
        }

        ZipFileEntry entry;
        entry.filename = filename.data();
        entry.compression_method = static_cast<uint16_t>(file_info.compression_method);
        entry.version_needed = static_cast<uint16_t>(file_info.version_needed);
        entry.flags = static_cast<uint16_t>(file_info.flag);
        entry.compressed_size = file_info.compressed_size;
        entry.uncompressed_size = file_info.uncompressed_size;
        entry.is_encrypted = (file_info.flag & 0x01) != 0;

        // Unix file attribute constants
        constexpr uint32_t UNIX_MODE_SHIFT = 16;
        constexpr uint16_t UNIX_MODE_MASK = 0xFFFF;
        constexpr uint16_t FILE_TYPE_MASK = 0xF000;
        constexpr uint16_t FILE_TYPE_SHIFT = 12;
        constexpr uint16_t SYMLINK_TYPE = 0xA;

        const uint16_t unix_mode = static_cast<uint16_t>((file_info.external_fa >> UNIX_MODE_SHIFT) & UNIX_MODE_MASK);
        const uint16_t file_type = (unix_mode & FILE_TYPE_MASK) >> FILE_TYPE_SHIFT;
        entry.is_symlink = (file_type == SYMLINK_TYPE);

        entries.push_back(entry);

        has_files = (unzGoToNextFile(uf) == UNZ_OK);
    }

    return entries;
}

bool Zipper::extractFile(const std::string& filename, std::vector<uint8_t>& output)
{
    if (!_zip_file)
        return false;

    unzFile uf = static_cast<unzFile>(_zip_file);

    if (unzLocateFile(uf, filename.c_str(), 0) != UNZ_OK)
        return false;

    unz_file_info file_info;
    if (unzGetCurrentFileInfo(uf, &file_info, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK)
        return false;

    if (unzOpenCurrentFile(uf) != UNZ_OK)
        return false;

    output.resize(file_info.uncompressed_size);

    const int32_t bytes_read = unzReadCurrentFile(uf, output.data(), static_cast<uint32_t>(output.size()));
    unzCloseCurrentFile(uf);

    if (bytes_read < 0 || static_cast<size_t>(bytes_read) != output.size())
        return false;

    return true;
}

bool Zipper::extractAll(const std::filesystem::path& destination)
{
    if (!_zip_file)
        return false;

    std::filesystem::create_directories(destination);

    auto entries = getEntries();
    for (const auto& entry : entries)
    {
        const std::filesystem::path file_path = destination / entry.filename;

        // Check if it's a directory (ends with /)
        if (!entry.filename.empty() && entry.filename.back() == '/')
        {
            std::filesystem::create_directories(file_path);
            continue;
        }

        // Create parent directories
        std::filesystem::create_directories(file_path.parent_path());

        // Extract file
        std::vector<uint8_t> data;
        if (!extractFile(entry.filename, data))
            return false;

        // Write to disk
        std::ofstream out(file_path, std::ios::binary);
        if (!out)
            return false;

        // Use static_cast instead of reinterpret_cast and explicit size conversion
        out.write(static_cast<const char*>(static_cast<const void*>(data.data())), static_cast<int64_t>(data.size()));
    }

    return true;
}

bool Zipper::addFile(const std::string& internal_path, const std::vector<uint8_t>& data, int32_t compression_level)
{
    if (!_zip_writer)
        return false;

    zipFile zf = static_cast<zipFile>(_zip_writer);

    const zip_fileinfo zi{};

    // Determine compression method
    const int32_t method = (compression_level > 0) ? Z_DEFLATED : 0;

    if (zipOpenNewFileInZip(zf, internal_path.c_str(), &zi, nullptr, 0, nullptr, 0, nullptr, method,
                            compression_level) != ZIP_OK)
        return false;

    if (zipWriteInFileInZip(zf, data.data(), static_cast<uint32_t>(data.size())) != ZIP_OK)
    {
        zipCloseFileInZip(zf);
        return false;
    }

    return zipCloseFileInZip(zf) == ZIP_OK;
}

bool Zipper::addFileFromDisk(const std::string& internal_path, const std::filesystem::path& source_path,
                             int32_t compression_level)
{
    std::ifstream file(source_path, std::ios::binary | std::ios::ate);
    if (!file)
        return false;

    const int64_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    // Use static_cast instead of reinterpret_cast
    if (!file.read(static_cast<char*>(static_cast<void*>(buffer.data())), size))
        return false;

    return addFile(internal_path, buffer, compression_level);
}

bool Zipper::isOpen() const
{
    return _zip_file != nullptr || _zip_writer != nullptr;
}

int32_t Zipper::getDiskCount() const
{
    if (_zip_path.empty())
        return -1;

    // Read EOCD to check disk numbers
    std::ifstream file(_zip_path, std::ios::binary);
    if (!file)
        return -1;

    // EOCD (End of Central Directory) constants
    constexpr uint32_t EOCD_SIGNATURE = 0x06054b50;
    constexpr size_t EOCD_MIN_SIZE = 22;
    constexpr size_t EOCD_DISK_NUMBER_OFFSET = 4;
    constexpr size_t EOCD_DISK_WITH_CD_OFFSET = 6;
    constexpr size_t MAX_ZIP_COMMENT_SIZE = 65535;

    constexpr size_t MAX_EOCD_SEARCH_SIZE = MAX_ZIP_COMMENT_SIZE + EOCD_MIN_SIZE;

    // Find EOCD signature at end of file
    file.seekg(-static_cast<std::streamoff>(EOCD_MIN_SIZE), std::ios::end);

    std::vector<uint8_t> buffer(EOCD_MIN_SIZE);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(EOCD_MIN_SIZE));

    // Helper lambda to read integers from buffer
    auto readInteger = [](const uint8_t* buf, size_t offset) -> uint32_t
    {
        uint32_t value = 0;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::memcpy(&value, buf + offset, sizeof(uint32_t));
        return value;
    };

    auto readInteger16 = [](const uint8_t* buf, size_t offset) -> uint16_t
    {
        uint16_t value = 0;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::memcpy(&value, buf + offset, sizeof(uint16_t));
        return value;
    };

    const uint32_t signature = readInteger(buffer.data(), 0);
    if (signature != EOCD_SIGNATURE)
    {
        // Might have comment, search backwards
        file.seekg(0, std::ios::end);
        auto file_size = file.tellg();
        const size_t search_size = std::min<size_t>(MAX_EOCD_SEARCH_SIZE, static_cast<size_t>(file_size));

        file.seekg(-static_cast<std::streamoff>(search_size), std::ios::end);
        std::vector<uint8_t> search_buffer(search_size);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        file.read(reinterpret_cast<char*>(search_buffer.data()), static_cast<std::streamsize>(search_size));

        bool found = false;
        for (size_t i = search_buffer.size() - EOCD_MIN_SIZE; i > 0; --i)
        {
            const uint32_t sig = readInteger(search_buffer.data(), i);
            if (sig == EOCD_SIGNATURE)
            {
                buffer.assign(search_buffer.begin() + static_cast<std::ptrdiff_t>(i),
                              search_buffer.begin() + static_cast<std::ptrdiff_t>(i + EOCD_MIN_SIZE));
                found = true;
                break;
            }
        }

        if (!found)
            return -1;
    }

    // Check disk numbers
    const uint16_t disk_number = readInteger16(buffer.data(), EOCD_DISK_NUMBER_OFFSET);
    const uint16_t disk_with_cd = readInteger16(buffer.data(), EOCD_DISK_WITH_CD_OFFSET);

    // Both should be the same for valid archives
    // Return the maximum of the two (disk numbers are 0-indexed, so add 1 for count)
    return static_cast<int32_t>(std::max(disk_number, disk_with_cd)) + 1;
}
