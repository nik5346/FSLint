#include "archive_checker.h"
#include "certificate.h"
#include "test_helpers.h"
#include "zipper.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <vector>

TEST_CASE("Zip Slip Detection", "[archive][security]")
{
    ArchiveChecker checker;

    SECTION("Normal path (PASS)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry entry{};
        entry.filename = "modelDescription.xml";
        entry.raw_filename = entry.filename;
        entries.push_back(entry);

        Certificate cert;
        checker.checkZipSlip(entries, cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("Relative path staying in root (PASS)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry entry{};
        entry.filename = "binaries/win64/model.dll";
        entry.raw_filename = entry.filename;
        entries.push_back(entry);

        Certificate cert;
        checker.checkZipSlip(entries, cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("Path with .. staying in root (PASS)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry entry{};
        entry.filename = "binaries/../modelDescription.xml";
        entry.raw_filename = entry.filename;
        entries.push_back(entry);

        Certificate cert;
        checker.checkZipSlip(entries, cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("Zip Slip path escaping root (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry entry{};
        entry.filename = "../../etc/passwd";
        entry.raw_filename = entry.filename;
        entries.push_back(entry);

        Certificate cert;
        checker.checkZipSlip(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "[SECURITY] Zip Slip Check"));
        CHECK(has_error_with_text(cert, "path escapes archive root"));
    }

    SECTION("Crafted Zip Slip path (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry entry{};
        entry.filename = "foo/../../etc/passwd";
        entry.raw_filename = entry.filename;
        entries.push_back(entry);

        Certificate cert;
        checker.checkZipSlip(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "[SECURITY] Zip Slip Check"));
        CHECK(has_error_with_text(cert, "path escapes archive root"));
    }
}

TEST_CASE("Zip Bomb Detection", "[archive][security]")
{
    ArchiveChecker checker;

    SECTION("Normal file ratio (PASS)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry entry{};
        entry.filename = "test.txt";
        entry.raw_filename = entry.filename;
        entry.compression_method = 8;
        entry.compressed_size = 500;
        entry.uncompressed_size = 1000;
        entries.push_back(entry);

        Certificate cert;
        checker.checkZipBomb(entries, cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("High ratio file (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry entry{};
        entry.filename = "bomb.txt";
        entry.raw_filename = entry.filename;
        entry.compression_method = 8;
        entry.compressed_size = 10;
        entry.uncompressed_size = 1000000;
        entries.push_back(entry);

        Certificate cert;
        checker.checkZipBomb(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "[SECURITY] Zip Bomb Check"));
        CHECK(has_error_with_text(cert, "excessive compression ratio"));
    }

    SECTION("Single file too large (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry entry{};
        entry.filename = "large.txt";
        entry.raw_filename = entry.filename;
        entry.compression_method = 8;
        entry.compressed_size = 500000000;
        entry.uncompressed_size = 1100000000;
        entries.push_back(entry);

        Certificate cert;
        checker.checkZipBomb(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "[SECURITY] Zip Bomb Check"));
        CHECK(has_error_with_text(cert, "uncompressed size exceeds 1 GB limit"));
    }

    SECTION("Total size too large (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        for (int i = 0; i < 11; ++i)
        {
            ZipFileEntry entry{};
            entry.filename = "file" + std::to_string(i) + ".txt";
            entry.raw_filename = entry.filename;
            entry.compression_method = 8;
            entry.compressed_size = 100000000;
            entry.uncompressed_size = 1000000000;
            entries.push_back(entry);
        }

        Certificate cert;
        checker.checkZipBomb(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "Total uncompressed size of archive exceeds 10 GB limit"));
    }
}

TEST_CASE("Duplicate and Case-Conflicting Names", "[archive][security]")
{
    ArchiveChecker checker;

    SECTION("Unique names (PASS)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry e1{}, e2{};
        e1.filename = "file1.txt";
        e1.raw_filename = e1.filename;
        e2.filename = "file2.txt";
        e2.raw_filename = e2.filename;
        entries.push_back(e1);
        entries.push_back(e2);

        Certificate cert;
        checker.checkDuplicateNames(entries, cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("Exact duplicate (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry e1{};
        e1.filename = "file.txt";
        e1.raw_filename = e1.filename;
        entries.push_back(e1);
        entries.push_back(e1);

        Certificate cert;
        checker.checkDuplicateNames(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "[SECURITY] Duplicate Entry Names Check"));
        CHECK(has_error_with_text(cert, "Duplicate entry name found"));
    }

    SECTION("Case conflict (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry e1{}, e2{};
        e1.filename = "Model.xml";
        e1.raw_filename = e1.filename;
        e2.filename = "model.xml";
        e2.raw_filename = e2.filename;
        entries.push_back(e1);
        entries.push_back(e2);

        Certificate cert;
        checker.checkDuplicateNames(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "[SECURITY] Duplicate Entry Names Check"));
        CHECK(has_error_with_text(cert, "Case-conflicting entry names found"));
    }
}

TEST_CASE("Overlapping Entries", "[archive][security]")
{
    ArchiveChecker checker;

    SECTION("Non-overlapping (PASS)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry e1{}, e2{};
        e1.filename = "file1.txt";
        e1.raw_filename = e1.filename;
        e1.compressed_size = 10;
        e1.offset = 0;
        e1.filename_length = 10;
        e2.filename = "file2.txt";
        e2.raw_filename = e2.filename;
        e2.compressed_size = 10;
        e2.offset = 100;
        e2.filename_length = 10;
        entries.push_back(e1);
        entries.push_back(e2);

        Certificate cert;
        checker.checkOverlappingEntries(entries, cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("Overlapping (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry e1{}, e2{};
        e1.filename = "file1.txt";
        e1.raw_filename = e1.filename;
        e1.compressed_size = 10;
        e1.offset = 0;
        e1.filename_length = 10;
        e2.filename = "file2.txt";
        e2.raw_filename = e2.filename;
        e2.compressed_size = 10;
        e2.offset = 40;
        e2.filename_length = 10;
        entries.push_back(e1);
        entries.push_back(e2);

        Certificate cert;
        checker.checkOverlappingEntries(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "[SECURITY] Overlapping File Entries Check"));
        CHECK(has_error_with_text(cert, "Overlapping file entries"));
    }
}

TEST_CASE("Backslash Normalization and Validation", "[archive]")
{
    ArchiveChecker checker;

    SECTION("Backslash in path (FAIL but normalized)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry entry{};
        entry.raw_filename = "binaries\\win64\\model.dll";
        entry.filename = "binaries/win64/model.dll";
        entry.filename_length = static_cast<uint16_t>(entry.raw_filename.length());
        entries.push_back(entry);

        Certificate cert;
        checker.checkPathFormat(entries, cert);

        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "contains backslashes"));

        Certificate cert2;
        checker.checkZipSlip(entries, cert2);
        CHECK_FALSE(has_fail(cert2));
    }

    SECTION("Duplicate names with different separators (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry e1{}, e2{};
        e1.raw_filename = "a\\b.txt";
        e1.filename = "a/b.txt";
        e2.raw_filename = "a/b.txt";
        e2.filename = "a/b.txt";
        entries.push_back(e1);
        entries.push_back(e2);

        Certificate cert;
        checker.checkDuplicateNames(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "Duplicate entry name found: 'a/b.txt'"));
    }
}
