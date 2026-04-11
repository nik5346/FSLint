#include "archive_checker.h"

#include "certificate.h"
#include "file_utils.h"
#include "zipper.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

void ArchiveChecker::validate(const std::filesystem::path& fmu_path, Certificate& cert) const
{
    cert.printSubsectionHeader("ARCHIVE VALIDATION");

    // Check if file exists
    if (!std::filesystem::exists(fmu_path))
    {
        std::cerr << "File does not exist: " << file_utils::pathToUtf8(fmu_path) << "\n";
        cert.printTestResult({"File Existence",
                              TestStatus::FAIL,
                              {std::format("File does not exist: {}", file_utils::pathToUtf8(fmu_path))}});
        cert.printSubsectionSummary(false);
        return;
    }

    // Check file extension
    checkFileExtension(fmu_path, cert);

    // Open ZIP file
    Zipper handler;
    if (!handler.open(fmu_path))
    {
        std::cerr << "Failed to open ZIP file: " << file_utils::pathToUtf8(fmu_path) << "\n";
        cert.printTestResult({"Archive Open",
                              TestStatus::FAIL,
                              {std::format("Failed to open ZIP file: {}", file_utils::pathToUtf8(fmu_path))}});
        cert.printSubsectionSummary(false);
        return;
    }

    // Check disk spanning
    checkDiskSpanning(handler, cert);

    // Get all entries
    const auto entries = handler.getEntries();
    if (entries.empty())
    {
        std::cerr << "ZIP file is empty: " << file_utils::pathToUtf8(fmu_path) << "\n";
        cert.printTestResult({"Archive Content",
                              TestStatus::FAIL,
                              {std::format("ZIP file is empty: {}", file_utils::pathToUtf8(fmu_path))}});
        cert.printSubsectionSummary(false);
        return;
    }

    // Run all validations on the entries
    checkCompressionMethods(entries, cert);
    if (cert.shouldAbort())
        return;
    checkVersionNeeded(entries, cert);
    if (cert.shouldAbort())
        return;
    checkLanguageEncodingFlag(entries, cert);
    if (cert.shouldAbort())
        return;
    checkEncryption(entries, cert);
    if (cert.shouldAbort())
        return;
    checkPathFormat(entries, cert);
    if (cert.shouldAbort())
        return;
    checkSymbolicLinks(entries, cert);
    if (cert.shouldAbort())
        return;
    checkGeneralPurposeBit3(entries, cert);
    if (cert.shouldAbort())
        return;

    // New security-focused checks
    checkZipSlip(entries, cert);
    if (cert.shouldAbort())
        return;
    checkZipBomb(entries, cert);
    if (cert.shouldAbort())
        return;
    checkDuplicateNames(entries, cert);
    if (cert.shouldAbort())
        return;
    checkOverlappingEntries(entries, cert);
    if (cert.shouldAbort())
        return;
    checkCentralDirectoryConsistency(fmu_path, entries, cert);
    if (cert.shouldAbort())
        return;
    checkEntryCountSanity(handler, entries, cert);
    if (cert.shouldAbort())
        return;
    checkExtraFieldsAndComments(handler, entries, cert);

    handler.close();

    cert.printSubsectionSummary(!cert.isFailed());
}

void ArchiveChecker::checkFileExtension(const std::filesystem::path& path, Certificate& cert) const
{
    TestResult ext_test{"File Extension Check", TestStatus::PASS, {}};

    if (path.extension() != ".fmu" && path.extension() != ".ssp")
    {
        ext_test.setStatus(TestStatus::FAIL);
        ext_test.getMessages().emplace_back("Model file must have .fmu or .ssp extension respectively.");
    }

    cert.printTestResult(ext_test);
}

void ArchiveChecker::checkCompressionMethods(const std::vector<ZipFileEntry>& entries, Certificate& cert) const
{
    TestResult test{"Compression Method Check", TestStatus::PASS, {}};

    for (const auto& entry : entries)
    {
        // ZIP compression methods
        constexpr size_t COMPRESSION_STORE = 0;
        constexpr size_t COMPRESSION_DEFLATE = 8;

        if (entry.compression_method != COMPRESSION_STORE && entry.compression_method != COMPRESSION_DEFLATE)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Invalid compression method for '{}': {} (only {}=store and {}=deflate allowed).",
                            entry.filename, entry.compression_method, COMPRESSION_STORE, COMPRESSION_DEFLATE));
        }
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkZipSlip(const std::vector<ZipFileEntry>& entries, Certificate& cert) const
{
    TestResult test{"[SECURITY] Zip Slip Check", TestStatus::PASS, {}};

    for (const auto& entry : entries)
    {
        // Lexically normalize the path to check for traversal.
        // We use an imaginary base directory to verify that the path stays within it.
        const std::filesystem::path entry_path(entry.filename);
        const std::filesystem::path base("/base");
        const std::filesystem::path normalized = (base / entry_path).lexically_normal();

        // Check if the normalized path still starts with the base.
        const auto [base_it, norm_it] = std::mismatch(base.begin(), base.end(), normalized.begin());

        if (base_it != base.end())
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Zip Slip detected in '{}': path escapes archive root.", entry.filename));
        }
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkZipBomb(const std::vector<ZipFileEntry>& entries, Certificate& cert) const
{
    TestResult test{"[SECURITY] Zip Bomb Check", TestStatus::PASS, {}};

    constexpr double MAX_RATIO = 100.0;
    constexpr uint64_t MAX_SINGLE_SIZE = 1024ULL * 1024ULL * 1024ULL;        // 1 GB
    constexpr uint64_t MAX_TOTAL_SIZE = 10ULL * 1024ULL * 1024ULL * 1024ULL; // 10 GB

    uint64_t total_uncompressed_size = 0;

    for (const auto& entry : entries)
    {
        total_uncompressed_size += entry.uncompressed_size;

        // Check single file size
        if (entry.uncompressed_size > MAX_SINGLE_SIZE)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("File '{}' uncompressed size exceeds 1 GB limit ({} bytes). Potential zip bomb.",
                            entry.filename, entry.uncompressed_size));
        }

        // Check compression ratio
        if (entry.compressed_size > 0)
        {
            const double ratio =
                static_cast<double>(entry.uncompressed_size) / static_cast<double>(entry.compressed_size);
            if (ratio > MAX_RATIO)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("File '{}' has excessive compression ratio ({:.2f}:1). Potential zip bomb.",
                                entry.filename, ratio));
            }
        }
    }

    if (total_uncompressed_size > MAX_TOTAL_SIZE)
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back(
            std::format("Total uncompressed size of archive exceeds 10 GB limit ({} bytes).", total_uncompressed_size));
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkDuplicateNames(const std::vector<ZipFileEntry>& entries, Certificate& cert) const
{
    TestResult test{"[SECURITY] Duplicate Entry Names Check", TestStatus::PASS, {}};

    std::vector<std::string> seen_names;
    std::vector<std::string> seen_names_lower;

    for (const auto& entry : entries)
    {
        // Exact duplicate
        if (std::ranges::find(seen_names, entry.filename) != seen_names.end())
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format("Duplicate entry name found: '{}'.", entry.filename));
        }
        else
        {
            seen_names.push_back(entry.filename);
        }

        // Case-insensitive duplicate
        std::string lower_name = entry.filename;
        std::ranges::transform(lower_name, lower_name.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(static_cast<int>(c))); });

        if (std::ranges::find(seen_names_lower, lower_name) != seen_names_lower.end())
        {
            // Only report if it's not an exact duplicate (already reported)
            if (std::ranges::count(seen_names, entry.filename) == 1)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format("Case-conflicting entry names found: '{}' collides with "
                                                            "another entry on case-insensitive filesystems.",
                                                            entry.filename));
            }
        }
        else
        {
            seen_names_lower.push_back(lower_name);
        }
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkOverlappingEntries(const std::vector<ZipFileEntry>& entries, Certificate& cert) const
{
    TestResult test{"[SECURITY] Overlapping File Entries Check", TestStatus::PASS, {}};

    struct Range
    {
        uint32_t start;
        uint32_t end;
        std::string filename;
    };

    std::vector<Range> ranges;
    for (const auto& entry : entries)
    {
        // Local File Header: 30 bytes fixed + filename + extra field
        // Compressed Data: compressed_size
        // We don't account for the Data Descriptor (optional) because we don't know its exact size/presence easily
        // but checking Header + Data is a good baseline.
        const uint32_t header_size = 30 + entry.filename_length + entry.extra_field_length;
        const uint32_t range_start = entry.offset;
        const uint32_t range_end = entry.offset + header_size + entry.compressed_size;

        for (const auto& r : ranges)
        {
            if (range_start < r.end && range_end > r.start)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Overlapping file entries: '{}' overlaps with '{}'.", entry.filename, r.filename));
            }
        }
        ranges.push_back({.start = range_start, .end = range_end, .filename = entry.filename});
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkCentralDirectoryConsistency(const std::filesystem::path& path,
                                                      const std::vector<ZipFileEntry>& entries, Certificate& cert) const
{
    TestResult test{"[SECURITY] Central Directory Consistency Check", TestStatus::PASS, {}};

    std::ifstream file(path, std::ios::binary);
    if (!file)
        return;

    for (const auto& entry : entries)
    {
        // Read Local File Header
        file.seekg(entry.offset, std::ios::beg);

        uint32_t signature = 0;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        file.read(reinterpret_cast<char*>(&signature), sizeof(signature));

        if (signature != 0x04034b50)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format(
                "Invalid Local File Header signature at offset {} for entry '{}'.", entry.offset, entry.raw_filename));
            continue;
        }

        // Check filename in local header
        file.seekg(entry.offset + 26, std::ios::beg);
        uint16_t local_filename_len = 0;
        uint16_t local_extra_len = 0;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        file.read(reinterpret_cast<char*>(&local_filename_len), sizeof(local_filename_len));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        file.read(reinterpret_cast<char*>(&local_extra_len), sizeof(local_extra_len));

        if (local_filename_len != entry.filename_length)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Filename length mismatch for '{}': Central Directory says {} but Local Header says {}.",
                            entry.raw_filename, entry.filename_length, local_filename_len));
        }

        std::string local_filename(local_filename_len, '\0');
        file.read(local_filename.data(), local_filename_len);

        if (local_filename != entry.raw_filename)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Filename mismatch for '{}': Central Directory says '{}' but Local Header says '{}'.",
                            entry.raw_filename, entry.raw_filename, local_filename));
        }

        if (local_extra_len != entry.extra_field_length)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Extra field length mismatch for '{}': Central Directory says {} but Local Header says {}.",
                            entry.raw_filename, entry.extra_field_length, local_extra_len));
        }

        // Optional: Check uncompressed size, compressed size, etc.
        // These are at offset 18 and 22 in LFH
        file.seekg(entry.offset + 18, std::ios::beg);
        uint32_t local_comp_size = 0;
        uint32_t local_uncomp_size = 0;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        file.read(reinterpret_cast<char*>(&local_comp_size), sizeof(local_comp_size));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        file.read(reinterpret_cast<char*>(&local_uncomp_size), sizeof(local_uncomp_size));

        // Note: If Bit 3 is set, these might be 0 in LFH and stored in Data Descriptor
        constexpr uint16_t DATA_DESCRIPTOR_BIT = 0x08;
        if (!(entry.flags & DATA_DESCRIPTOR_BIT))
        {
            if (local_comp_size != entry.compressed_size)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "Compressed size mismatch for '{}': Central Directory says {} but Local Header says {}.",
                    entry.filename, entry.compressed_size, local_comp_size));
            }
            if (local_uncomp_size != entry.uncompressed_size)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format(
                    "Uncompressed size mismatch for '{}': Central Directory says {} but Local Header says {}.",
                    entry.filename, entry.uncompressed_size, local_uncomp_size));
            }
        }
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkEntryCountSanity(const Zipper& handler, const std::vector<ZipFileEntry>& entries,
                                           Certificate& cert) const
{
    TestResult test{"[SECURITY] Total Entry Count Sanity Check", TestStatus::PASS, {}};

    const int32_t reported_count = handler.getReportedEntryCount();
    if (reported_count != static_cast<int32_t>(entries.size()))
    {
        test.setStatus(TestStatus::FAIL);

        test.getMessages().emplace_back(
            std::format("Total entry count mismatch: Central Directory reports {} entries, but {} were found.",
                        reported_count, entries.size()));
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkExtraFieldsAndComments(const Zipper& handler, const std::vector<ZipFileEntry>& entries,
                                                 Certificate& cert) const
{
    TestResult test{"[SECURITY] Extra Field and Comment Integrity Check", TestStatus::PASS, {}};

    // Comment check
    const std::string comment = handler.getComment();
    // getComment() already validates that the actual bytes exist in the file.
    // If we wanted to check for "oversized" but valid comments:
    if (comment.size() > 2048) // Arbitrary limit for "sanity", though ZIP allows 64K
    {
        test.setStatus(TestStatus::WARNING);
        test.getMessages().emplace_back(std::format("Archive comment is unusually large ({} bytes).", comment.size()));
    }

    for (const auto& entry : entries)
    {
        // Sanity check for extra field length
        if (entry.extra_field_length > 4096)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format("Extra field for '{}' is excessively large ({} bytes). This "
                                                        "may indicate a malformed or malicious archive.",
                                                        entry.filename, entry.extra_field_length));
        }
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkVersionNeeded(const std::vector<ZipFileEntry>& entries, Certificate& cert) const
{
    TestResult test{"Version Needed Check", TestStatus::PASS, {}};

    constexpr size_t VERSION_CONVERSION_FACTOR = 10;
    constexpr size_t MAX_ZIP_VERSION = 2 * VERSION_CONVERSION_FACTOR; // 2.0 = 20

    for (const auto& entry : entries)
    {
        if (entry.version_needed > MAX_ZIP_VERSION)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format(
                "Version needed to extract for '{}' is {}.{} (maximum allowed is 2.0).", entry.filename,
                entry.version_needed / VERSION_CONVERSION_FACTOR, entry.version_needed % VERSION_CONVERSION_FACTOR));
        }
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkLanguageEncodingFlag(const std::vector<ZipFileEntry>& entries, Certificate& cert) const
{
    TestResult test{"Language Encoding Flag Check", TestStatus::PASS, {}};
    constexpr uint16_t LANGUAGE_ENCODING_BIT = 0x800; // Bit 11
    constexpr unsigned char MAX_ASCII_VALUE = 127;

    for (const auto& entry : entries)
    {
        const bool bit11_set = (entry.flags & LANGUAGE_ENCODING_BIT) != 0;
        const bool has_non_ascii =
            std::ranges::any_of(entry.raw_filename, [](unsigned char c) { return c > MAX_ASCII_VALUE; });

        if (has_non_ascii && !bit11_set)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format(
                "Language encoding flag (bit 11) must be set for '{}' because it contains non-ASCII characters. "
                "This may cause compatibility issues on some platforms.",
                entry.raw_filename));
        }
        else if (!has_non_ascii && bit11_set)
        {
            if (test.getStatus() != TestStatus::FAIL)
                test.setStatus(TestStatus::WARNING);
            test.getMessages().emplace_back(std::format(
                "Language encoding flag (bit 11) is set for '{}' but it does not contain non-ASCII characters. "
                "While not strictly invalid, this may indicate a misconfiguration (for maximum portability, keeping "
                "this bit at 0 is recommended).",
                entry.raw_filename));
        }
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkEncryption(const std::vector<ZipFileEntry>& entries, Certificate& cert) const
{
    TestResult test{"Encryption Check", TestStatus::PASS, {}};

    for (const auto& entry : entries)
    {
        if (entry.is_encrypted)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("File '{}' is encrypted (encryption not allowed).", entry.filename));
        }
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkPathFormat(const std::vector<ZipFileEntry>& entries, Certificate& cert) const
{
    TestResult test{"Path Format Check", TestStatus::PASS, {}};

    // ASCII character range
    constexpr unsigned char MAX_ASCII_VALUE = 127;

    for (const auto& entry : entries)
    {
        const std::string& path = entry.raw_filename;

        // Check for control characters (U+0000–U+001F)
        for (const unsigned char c : path)
        {
            if (c <= 0x1F)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(
                    std::format("Path '{}' contains illegal control character (U+00{:02X}).", path, c));
                break;
            }
        }

        // Check for Windows-illegal characters (< > " | ? *)
        // Note: ':' is checked separately as it indicates drive/device paths.
        // Note: '/' and '\' are handled separately.
        const std::string illegal_chars = "<>\"|?*";
        for (const char c : illegal_chars)
        {
            if (path.find(c) != std::string::npos)
            {
                test.setStatus(TestStatus::FAIL);
                test.getMessages().emplace_back(std::format("Path '{}' contains illegal character '{}'.", path, c));
            }
        }

        // Check for backslashes (wrong path separator)
        if (path.find('\\') != std::string::npos)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(std::format("Path '{}' contains backslashes '\\' (only forward slashes '/' "
                                                        "allowed per ZIP specification section 4.4.17).",
                                                        path));
        }

        // Check for non-ASCII characters
        const bool has_non_ascii = std::ranges::any_of(path, [](unsigned char c) { return c > MAX_ASCII_VALUE; });
        constexpr uint16_t LANGUAGE_ENCODING_BIT = 0x800; // Bit 11
        const bool bit11_set = (entry.flags & LANGUAGE_ENCODING_BIT) != 0;

        if (has_non_ascii && !bit11_set)
        {
            test.setStatus(TestStatus::WARNING);
            test.getMessages().emplace_back(
                std::format("Path '{}' contains non-ASCII characters but language encoding flag (bit 11) is not set. "
                            "This may cause compatibility issues on some platforms.",
                            path));
        }

        // Check for leading slash (absolute path)
        if (!path.empty() && path[0] == '/')
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Path '{}' starts with '/' (absolute paths not allowed).", path));
        }

        // Check for drive or device letter (Windows-style paths)
        if (path.find(':') != std::string::npos)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Path '{}' contains ':' (drive letters or device paths not allowed).", path));
        }

        // Check for parent directory traversal
        if (path.find("../") != std::string::npos || path.find("..\\") != std::string::npos)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Path '{}' contains '..' (parent directory traversal not allowed).", path));
        }

        // Check for current directory reference at start
        if (path.size() >= 2 && path[0] == '.' && path[1] == '/')
        {
            test.setStatus(TestStatus::WARNING);
            test.getMessages().emplace_back(
                std::format("Path '{}' starts with './' (redundant current directory reference).", path));
        }

        // Check for multiple consecutive slashes
        if (path.find("//") != std::string::npos)
        {
            test.setStatus(TestStatus::WARNING);
            test.getMessages().emplace_back(
                std::format("Path '{}' contains '//' (multiple consecutive slashes).", path));
        }
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkSymbolicLinks(const std::vector<ZipFileEntry>& entries, Certificate& cert) const
{
    TestResult test{"Symbolic Links Check", TestStatus::PASS, {}};

    for (const auto& entry : entries)
    {
        if (entry.is_symlink) // You'll need to add this field to ZipFileEntry
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("Symbolic link found: '{}' (links not allowed in FMU/SSP archives).", entry.filename));
        }
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkGeneralPurposeBit3(const std::vector<ZipFileEntry>& entries, Certificate& cert) const
{
    TestResult test{"General Purpose Bit 3 Check", TestStatus::PASS, {}};

    // ZIP general purpose bit flags
    constexpr uint16_t DATA_DESCRIPTOR_BIT = 0x08;

    for (const auto& entry : entries)
    {
        // Bit 3 is the data descriptor bit
        const bool bit3_set = (entry.flags & DATA_DESCRIPTOR_BIT) != 0;

        // ZIP compression methods
        constexpr size_t COMPRESSION_DEFLATE = 8;

        if (bit3_set && entry.compression_method != COMPRESSION_DEFLATE)
        {
            test.setStatus(TestStatus::FAIL);
            test.getMessages().emplace_back(
                std::format("General purpose bit 3 is set for '{}' but compression method is not deflate ({}).",
                            entry.filename, COMPRESSION_DEFLATE));
        }
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkDiskSpanning(Zipper& handler, Certificate& cert) const
{
    TestResult test{"Disk Spanning Check", TestStatus::PASS, {}};

    if (handler.getDiskCount() != 1)
    {
        test.setStatus(TestStatus::FAIL);
        test.getMessages().emplace_back("Split or spanned ZIP archives are not allowed.");
    }

    cert.printTestResult(test);
}
