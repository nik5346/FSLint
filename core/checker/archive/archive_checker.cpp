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
        cert.printTestResult(
            {"File Existence", TestStatus::FAIL, {"File does not exist: " + file_utils::pathToUtf8(fmu_path)}});
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
        cert.printTestResult(
            {"Archive Open", TestStatus::FAIL, {"Failed to open ZIP file: " + file_utils::pathToUtf8(fmu_path)}});
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
        cert.printTestResult(
            {"Archive Content", TestStatus::FAIL, {"ZIP file is empty: " + file_utils::pathToUtf8(fmu_path)}});
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
        ext_test.status = TestStatus::FAIL;
        ext_test.messages.push_back("model file must have .fmu or .ssp extension respectively.");
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
            test.status = TestStatus::FAIL;
            test.messages.push_back("Invalid compression method for '" + entry.filename +
                                    "': " + std::to_string(entry.compression_method) + " (only " +
                                    std::to_string(COMPRESSION_STORE) + "=store and " +
                                    std::to_string(COMPRESSION_DEFLATE) + "=deflate allowed).");
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
            test.status = TestStatus::FAIL;
            test.messages.push_back("Zip Slip detected in '" + entry.filename + "': path escapes archive root.");
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
            test.status = TestStatus::FAIL;
            test.messages.push_back("File '" + entry.filename + "' uncompressed size exceeds 1 GB limit (" +
                                    std::to_string(entry.uncompressed_size) + " bytes).");
        }

        // Check compression ratio
        if (entry.compressed_size > 0)
        {
            const double ratio =
                static_cast<double>(entry.uncompressed_size) / static_cast<double>(entry.compressed_size);
            if (ratio > MAX_RATIO)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("File '" + entry.filename + "' has excessive compression ratio (" +
                                        std::to_string(ratio) + ":1). Potential zip bomb.");
            }
        }
    }

    if (total_uncompressed_size > MAX_TOTAL_SIZE)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Total uncompressed size of archive exceeds 10 GB limit (" +
                                std::to_string(total_uncompressed_size) + " bytes).");
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
        if (std::find(seen_names.begin(), seen_names.end(), entry.filename) != seen_names.end())
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Duplicate entry name found: '" + entry.filename + "'.");
        }
        else
        {
            seen_names.push_back(entry.filename);
        }

        // Case-insensitive duplicate
        std::string lower_name = entry.filename;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(static_cast<int>(c))); });

        if (std::find(seen_names_lower.begin(), seen_names_lower.end(), lower_name) != seen_names_lower.end())
        {
            // Only report if it's not an exact duplicate (already reported)
            if (std::count(seen_names.begin(), seen_names.end(), entry.filename) == 1)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Case-conflicting entry names found: '" + entry.filename +
                                        "' collides with another entry on case-insensitive filesystems.");
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
                test.status = TestStatus::FAIL;
                test.messages.push_back("Overlapping file entries: '" + entry.filename + "' overlaps with '" +
                                        r.filename + "'.");
            }
        }
        ranges.push_back({range_start, range_end, entry.filename});
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
        file.read(reinterpret_cast<char*>(&signature), sizeof(signature));

        if (signature != 0x04034b50)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Invalid Local File Header signature at offset " + std::to_string(entry.offset) +
                                    " for entry '" + entry.filename + "'.");
            continue;
        }

        // Check filename in local header
        file.seekg(entry.offset + 26, std::ios::beg);
        uint16_t local_filename_len = 0;
        uint16_t local_extra_len = 0;
        file.read(reinterpret_cast<char*>(&local_filename_len), sizeof(local_filename_len));
        file.read(reinterpret_cast<char*>(&local_extra_len), sizeof(local_extra_len));

        if (local_filename_len != entry.filename_length)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Filename length mismatch for '" + entry.filename + "': Central Directory says " +
                                    std::to_string(entry.filename_length) + " but Local Header says " +
                                    std::to_string(local_filename_len) + ".");
        }

        std::string local_filename(local_filename_len, '\0');
        file.read(local_filename.data(), local_filename_len);

        if (local_filename != entry.filename)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Filename mismatch for '" + entry.filename + "': Central Directory says '" +
                                    entry.filename + "' but Local Header says '" + local_filename + "'.");
        }

        if (local_extra_len != entry.extra_field_length)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Extra field length mismatch for '" + entry.filename +
                                    "': Central Directory says " + std::to_string(entry.extra_field_length) +
                                    " but Local Header says " + std::to_string(local_extra_len) + ".");
        }

        // Optional: Check uncompressed size, compressed size, etc.
        // These are at offset 18 and 22 in LFH
        file.seekg(entry.offset + 18, std::ios::beg);
        uint32_t local_comp_size = 0;
        uint32_t local_uncomp_size = 0;
        file.read(reinterpret_cast<char*>(&local_comp_size), sizeof(local_comp_size));
        file.read(reinterpret_cast<char*>(&local_uncomp_size), sizeof(local_uncomp_size));

        // Note: If Bit 3 is set, these might be 0 in LFH and stored in Data Descriptor
        constexpr uint16_t DATA_DESCRIPTOR_BIT = 0x08;
        if (!(entry.flags & DATA_DESCRIPTOR_BIT))
        {
            if (local_comp_size != entry.compressed_size)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Compressed size mismatch for '" + entry.filename +
                                        "': Central Directory says " + std::to_string(entry.compressed_size) +
                                        " but Local Header says " + std::to_string(local_comp_size) + ".");
            }
            if (local_uncomp_size != entry.uncompressed_size)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Uncompressed size mismatch for '" + entry.filename +
                                        "': Central Directory says " + std::to_string(entry.uncompressed_size) +
                                        " but Local Header says " + std::to_string(local_uncomp_size) + ".");
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
        test.status = TestStatus::FAIL;
        test.messages.push_back("Total entry count mismatch: Central Directory reports " +
                                std::to_string(reported_count) + " entries, but " + std::to_string(entries.size()) +
                                " were found.");
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
        test.status = TestStatus::WARNING;
        test.messages.push_back("Archive comment is unusually large (" + std::to_string(comment.size()) + " bytes).");
    }

    for (const auto& entry : entries)
    {
        // Sanity check for extra field length
        if (entry.extra_field_length > 4096)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Extra field for '" + entry.filename + "' is suspiciously large (" +
                                    std::to_string(entry.extra_field_length) + " bytes).");
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
            test.status = TestStatus::FAIL;
            test.messages.push_back("Version needed to extract for '" + entry.filename + "' is " +
                                    std::to_string(entry.version_needed / VERSION_CONVERSION_FACTOR) + "." +
                                    std::to_string(entry.version_needed % VERSION_CONVERSION_FACTOR) +
                                    " (maximum allowed is 2.0).");
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
        const bool has_non_ascii = std::any_of(entry.filename.begin(), entry.filename.end(),
                                               [](unsigned char c) { return c > MAX_ASCII_VALUE; });

        if (has_non_ascii && !bit11_set)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Language encoding flag (bit 11) must be set for '" + entry.filename +
                                    "' because it contains non-ASCII characters.");
        }
        else if (!has_non_ascii && bit11_set)
        {
            if (test.status != TestStatus::FAIL)
                test.status = TestStatus::WARNING;
            test.messages.push_back("Language encoding flag (bit 11) is set for '" + entry.filename +
                                    "' but it only contains ASCII characters (for maximum portability, keeping this "
                                    "bit at 0 is recommended).");
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
            test.status = TestStatus::FAIL;
            test.messages.push_back("File '" + entry.filename + "' is encrypted (encryption not allowed).");
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
        const std::string& path = entry.filename;

        // Check for control characters (U+0000–U+001F)
        for (const unsigned char c : path)
        {
            if (c <= 0x1F)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Path '" + path + "' contains illegal control character (U+00" +
                                        (c < 0x10 ? "0" : "") + std::format("{:X}", c) + ").");
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
                test.status = TestStatus::FAIL;
                test.messages.push_back("Path '" + path + "' contains illegal character '" + c + "'.");
            }
        }

        // Check for backslashes (wrong path separator)
        if (path.find('\\') != std::string::npos)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Backslash '\\' found in path '" + path +
                                    "' (only forward slashes '/' allowed per ZIP specification section 4.4.17).");
        }

        // Check for non-ASCII characters
        const bool has_non_ascii =
            std::any_of(path.begin(), path.end(), [](unsigned char c) { return c > MAX_ASCII_VALUE; });
        constexpr uint16_t LANGUAGE_ENCODING_BIT = 0x800; // Bit 11
        const bool bit11_set = (entry.flags & LANGUAGE_ENCODING_BIT) != 0;

        if (has_non_ascii && !bit11_set)
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Non-ASCII characters in path '" + path +
                                    "' (may cause compatibility issues on different operating systems).");
        }

        // Check for leading slash (absolute path)
        if (!path.empty() && path[0] == '/')
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Path '" + path + "' must not start with '/' (absolute paths not allowed).");
        }

        // Check for drive or device letter (Windows-style paths)
        if (path.find(':') != std::string::npos)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Path '" + path +
                                    "' must not contain ':' (drive letters or device paths not allowed).");
        }

        // Check for parent directory traversal
        if (path.find("../") != std::string::npos || path.find("..\\") != std::string::npos)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Path '" + path + "' contains '..' (parent directory traversal not allowed).");
        }

        // Check for current directory reference at start
        if (path.size() >= 2 && path[0] == '.' && path[1] == '/')
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Path '" + path + "' starts with './' (redundant current directory reference).");
        }

        // Check for multiple consecutive slashes
        if (path.find("//") != std::string::npos)
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Path '" + path + "' contains '//' (multiple consecutive slashes).");
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
            test.status = TestStatus::FAIL;
            test.messages.push_back("Symbolic link found: '" + entry.filename +
                                    "' (links not allowed in FMU/SSP archives).");
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
            test.status = TestStatus::FAIL;
            test.messages.push_back("General purpose bit 3 is set for '" + entry.filename +
                                    "' but compression method is not deflate (" + std::to_string(COMPRESSION_DEFLATE) +
                                    ").");
        }
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkDiskSpanning(Zipper& handler, Certificate& cert) const
{
    TestResult test{"Disk Spanning Check", TestStatus::PASS, {}};

    if (handler.getDiskCount() != 1)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Split or spanned ZIP archives are not allowed.");
    }

    cert.printTestResult(test);
}