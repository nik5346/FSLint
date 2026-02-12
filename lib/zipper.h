#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct ZipFileEntry
{
    std::string filename;
    uint16_t compression_method = 0;
    uint16_t version_needed = 0;
    uint16_t flags = 0;
    uint32_t compressed_size = 0;
    uint32_t uncompressed_size = 0;
    bool is_encrypted = false;
    bool is_symlink = false;
};

class Zipper
{
  public:
    // Constants
    static constexpr int32_t DEFAULT_COMPRESSION_LEVEL = 6;

    Zipper() = default;
    ~Zipper();

    // Prevent copying
    Zipper(const Zipper&) = delete;
    Zipper& operator=(const Zipper&) = delete;
    Zipper(Zipper&&) = delete;
    Zipper& operator=(Zipper&&) = delete;

    // Reading operations
    bool open(const std::filesystem::path& zip_path);

    // Writing operations
    bool create(const std::filesystem::path& zip_path);

    void close();

    std::vector<ZipFileEntry> getEntries() const;
    bool extractFile(const std::string& filename, std::vector<uint8_t>& output);
    bool extractAll(const std::filesystem::path& destination);

    bool addFile(const std::string& internal_path, const std::vector<uint8_t>& data,
                 int32_t compression_level = DEFAULT_COMPRESSION_LEVEL);

    bool addFileFromDisk(const std::string& internal_path, const std::filesystem::path& source_path,
                         int32_t compression_level = DEFAULT_COMPRESSION_LEVEL);

    // Utility
    bool isOpen() const;
    int32_t getDiskCount() const;

  private:
    void* _zip_file = nullptr;   // unzFile for reading
    void* _zip_writer = nullptr; // zipFile for writing
    std::filesystem::path _zip_path;
};