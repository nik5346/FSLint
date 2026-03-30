#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

/// @brief Metadata for a single file entry in a ZIP archive.
struct ZipFileEntry
{
    std::string filename;        ///< Path within the archive.
    uint16_t compression_method; ///< Method used (e.g., 8 for Deflate).
    uint16_t version_needed;     ///< Minimum ZIP version to extract.
    uint16_t flags;              ///< General purpose bit flags.
    uint32_t compressed_size;    ///< Size in archive.
    uint32_t uncompressed_size;  ///< Original size.
    uint32_t offset;             ///< Offset of the local file header.
    uint16_t filename_length;    ///< Length of the filename.
    uint16_t extra_field_length; ///< Length of the extra field.
    bool is_encrypted;           ///< True if encrypted.
    bool is_symlink;             ///< True if symbolic link.
};

/// @brief Wrapper for ZIP archive operations (reading and writing).
class Zipper
{
  public:
    /// @brief Default compression level for new files.
    static constexpr int32_t DEFAULT_COMPRESSION_LEVEL = 6;

    /// @brief Constructor.
    Zipper() = default;

    /// @brief Destructor (closes open files).
    ~Zipper();

    // Prevent copying
    Zipper(const Zipper&) = delete;
    Zipper& operator=(const Zipper&) = delete;
    Zipper(Zipper&&) = delete;
    Zipper& operator=(Zipper&&) = delete;

    /// @brief Opens an existing archive for reading.
    /// @param zip_path Path to file.
    /// @return True if opened.
    bool open(const std::filesystem::path& zip_path);

    /// @brief Creates a new archive for writing.
    /// @param zip_path Path to file.
    /// @return True if created.
    bool create(const std::filesystem::path& zip_path);

    /// @brief Closes open archive.
    void close();

    /// @brief Gets list of all files in archive.
    /// @return Vector of entries.
    std::vector<ZipFileEntry> getEntries() const;

    /// @brief Extracts a single file to a memory buffer.
    /// @param filename Path in archive.
    /// @param output Output buffer.
    /// @return True if successful.
    bool extractFile(const std::string& filename, std::vector<uint8_t>& output);

    /// @brief Extracts all files to a directory.
    /// @param destination Target directory.
    /// @return True if successful.
    bool extractAll(const std::filesystem::path& destination);

    /// @brief Adds a file from memory to the archive.
    /// @param internal_path Path in archive.
    /// @param data Data to add.
    /// @param compression_level Compression level.
    /// @return True if successful.
    bool addFile(const std::string& internal_path, const std::vector<uint8_t>& data,
                 int32_t compression_level = DEFAULT_COMPRESSION_LEVEL);

    /// @brief Adds a file from disk to the archive.
    /// @param internal_path Path in archive.
    /// @param source_path Path on disk.
    /// @param compression_level Compression level.
    /// @return True if successful.
    bool addFileFromDisk(const std::string& internal_path, const std::filesystem::path& source_path,
                         int32_t compression_level = DEFAULT_COMPRESSION_LEVEL);

    /// @brief Checks if archive is open.
    /// @return True if open.
    bool isOpen() const;

    /// @brief Gets the number of disks (spanning support).
    /// @return Disk count.
    int32_t getDiskCount() const;

    /// @brief Gets the total number of entries reported in the EOCD.
    /// @return Entry count, or -1 on error.
    int32_t getReportedEntryCount() const;

    /// @brief Gets the archive comment.
    /// @return The comment string.
    std::string getComment() const;

  private:
    void* _zip_file = nullptr;   // unzFile for reading
    void* _zip_writer = nullptr; // zipFile for writing
    std::filesystem::path _zip_path;
};
