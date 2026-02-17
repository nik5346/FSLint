#include "archive_checker.h"

#include "certificate.h"
#include "zipper.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

void ArchiveChecker::validate(const std::filesystem::path& fmu_path, Certificate& cert)
{
    cert.printSubsectionHeader("ARCHIVE VALIDATION");

    // Check if file exists
    if (!std::filesystem::exists(fmu_path))
    {
        std::cerr << "File does not exist: " << fmu_path << "\n";
        cert.printTestResult({"File Existence", TestStatus::FAIL, {"File does not exist: " + fmu_path.string()}});
        cert.printSubsectionSummary(false);
        return;
    }

    // Check file extension
    checkFileExtension(fmu_path, cert);

    // Open ZIP file
    Zipper handler;
    if (!handler.open(fmu_path))
    {
        std::cerr << "Failed to open ZIP file: " << fmu_path << "\n";
        cert.printTestResult({"Archive Open", TestStatus::FAIL, {"Failed to open ZIP file: " + fmu_path.string()}});
        cert.printSubsectionSummary(false);
        return;
    }

    // Check disk spanning
    checkDiskSpanning(handler, cert);

    // Get all entries
    const auto entries = handler.getEntries();
    if (entries.empty())
    {
        std::cerr << "ZIP file is empty: " << fmu_path << "\n";
        cert.printTestResult({"Archive Content", TestStatus::FAIL, {"ZIP file is empty: " + fmu_path.string()}});
        cert.printSubsectionSummary(false);
        return;
    }

    // Run all validations on the entries
    checkCompressionMethods(entries, cert);
    checkVersionNeeded(entries, cert);
    checkLanguageEncodingFlag(entries, cert);
    checkEncryption(entries, cert);
    checkPathFormat(entries, cert);
    checkSymbolicLinks(entries, cert);
    checkGeneralPurposeBit3(entries, cert);

    handler.close();

    cert.printSubsectionSummary(!cert.isFailed());
}

void ArchiveChecker::checkFileExtension(const std::filesystem::path& path, Certificate& cert)
{
    TestResult ext_test{"File Extension Check", TestStatus::PASS, {}};

    if (path.extension() != ".fmu" && path.extension() != ".ssp")
    {
        ext_test.status = TestStatus::FAIL;
        ext_test.messages.push_back("model file must have .fmu or .ssp extension respectively.");
    }

    cert.printTestResult(ext_test);
}

void ArchiveChecker::checkCompressionMethods(const std::vector<ZipFileEntry>& entries, Certificate& cert)
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

void ArchiveChecker::checkVersionNeeded(const std::vector<ZipFileEntry>& entries, Certificate& cert)
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

void ArchiveChecker::checkLanguageEncodingFlag(const std::vector<ZipFileEntry>& entries, Certificate& cert)
{
    TestResult test{"Language Encoding Flag Check", TestStatus::PASS, {}};
    constexpr uint16_t LANGUAGE_ENCODING_BIT = 0x800; // Bit 11

    for (const auto& entry : entries)
    {
        if (entry.flags & LANGUAGE_ENCODING_BIT)
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Language encoding flag (bit 11) is set for '" + entry.filename +
                                    "' (for maximum portability, keeping this bit at 0 is recommended).");
        }
    }

    cert.printTestResult(test);
}

void ArchiveChecker::checkEncryption(const std::vector<ZipFileEntry>& entries, Certificate& cert)
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

void ArchiveChecker::checkPathFormat(const std::vector<ZipFileEntry>& entries, Certificate& cert)
{
    TestResult test{"Path Format Check", TestStatus::PASS, {}};

    // ASCII character range
    constexpr unsigned char MAX_ASCII_VALUE = 127;

    for (const auto& entry : entries)
    {
        const std::string& path = entry.filename;

        // Check for backslashes (wrong path separator)
        if (path.find('\\') != std::string::npos)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Backslash '\\' found in path '" + path +
                                    "' (only forward slashes '/' allowed per ZIP specification section 4.4.17).");
        }

        // Check for non-ASCII characters
        bool has_non_ascii = std::any_of(path.begin(), path.end(), [](unsigned char c) { return c > MAX_ASCII_VALUE; });
        if (has_non_ascii)
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

void ArchiveChecker::checkSymbolicLinks(const std::vector<ZipFileEntry>& entries, Certificate& cert)
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

void ArchiveChecker::checkGeneralPurposeBit3(const std::vector<ZipFileEntry>& entries, Certificate& cert)
{
    TestResult test{"General Purpose Bit 3 Check", TestStatus::PASS, {}};

    // ZIP general purpose bit flags
    constexpr uint16_t DATA_DESCRIPTOR_BIT = 0x08;

    for (const auto& entry : entries)
    {
        // Bit 3 is the data descriptor bit
        bool bit3_set = (entry.flags & DATA_DESCRIPTOR_BIT) != 0;

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

void ArchiveChecker::checkDiskSpanning(Zipper& handler, Certificate& cert)
{
    TestResult test{"Disk Spanning Check", TestStatus::PASS, {}};

    if (handler.getDiskCount() != 1)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Split or spanned ZIP archives are not allowed.");
    }

    cert.printTestResult(test);
}