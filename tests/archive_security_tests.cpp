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
        entries.push_back({"modelDescription.xml", 0, 20, 0, 0, 0, 0, 0, 0, false, false});

        Certificate cert;
        checker.checkZipSlip(entries, cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("Relative path staying in root (PASS)")
    {
        std::vector<ZipFileEntry> entries;
        entries.push_back({"binaries/win64/model.dll", 0, 20, 0, 0, 0, 0, 0, 0, false, false});

        Certificate cert;
        checker.checkZipSlip(entries, cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("Path with .. staying in root (PASS)")
    {
        std::vector<ZipFileEntry> entries;
        entries.push_back({"binaries/../modelDescription.xml", 0, 20, 0, 0, 0, 0, 0, 0, false, false});

        Certificate cert;
        checker.checkZipSlip(entries, cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("Zip Slip path escaping root (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        entries.push_back({"../../etc/passwd", 0, 20, 0, 0, 0, 0, 0, 0, false, false});

        Certificate cert;
        checker.checkZipSlip(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "[SECURITY] Zip Slip Check"));
        CHECK(has_error_with_text(cert, "path escapes archive root"));
    }

    SECTION("Crafted Zip Slip path (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        entries.push_back({"foo/../../etc/passwd", 0, 20, 0, 0, 0, 0, 0, 0, false, false});

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
        // 1000 compressed to 500 (2:1 ratio)
        entries.push_back({"test.txt", 8, 20, 0, 500, 1000, 0, 0, 0, false, false});

        Certificate cert;
        checker.checkZipBomb(entries, cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("High ratio file (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        // 1,000,000 compressed to 10 (100,000:1 ratio)
        entries.push_back({"bomb.txt", 8, 20, 0, 10, 1000000, 0, 0, 0, false, false});

        Certificate cert;
        checker.checkZipBomb(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "[SECURITY] Zip Bomb Check"));
        CHECK(has_error_with_text(cert, "excessive compression ratio"));
    }

    SECTION("Single file too large (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        // 1.1 GB uncompressed
        entries.push_back({"large.txt", 8, 20, 0, 500000000, 1100000000, 0, 0, 0, false, false});

        Certificate cert;
        checker.checkZipBomb(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "[SECURITY] Zip Bomb Check"));
        CHECK(has_error_with_text(cert, "uncompressed size exceeds 1 GB limit"));
    }

    SECTION("Total size too large (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        // 11 files of 1 GB each = 11 GB total
        for (int i = 0; i < 11; ++i)
        {
            entries.push_back(
                {"file" + std::to_string(i) + ".txt", 8, 20, 0, 100000000, 1000000000, 0, 0, 0, false, false});
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
        entries.push_back({"file1.txt", 0, 20, 0, 0, 0, 0, 0, 0, false, false});
        entries.push_back({"file2.txt", 0, 20, 0, 0, 0, 0, 0, 0, false, false});

        Certificate cert;
        checker.checkDuplicateNames(entries, cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("Exact duplicate (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        entries.push_back({"file.txt", 0, 20, 0, 0, 0, 0, 0, 0, false, false});
        entries.push_back({"file.txt", 0, 20, 0, 0, 0, 0, 0, 0, false, false});

        Certificate cert;
        checker.checkDuplicateNames(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "[SECURITY] Duplicate Entry Names Check"));
        CHECK(has_error_with_text(cert, "Duplicate entry name found"));
    }

    SECTION("Case conflict (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        entries.push_back({"Model.xml", 0, 20, 0, 0, 0, 0, 0, 0, false, false});
        entries.push_back({"model.xml", 0, 20, 0, 0, 0, 0, 0, 0, false, false});

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
        // Entry 1 at 0, size 30+10+10 = 50
        entries.push_back({"file1.txt", 0, 20, 0, 10, 10, 0, 10, 0, false, false});
        // Entry 2 at 100, size 30+10+10 = 50
        entries.push_back({"file2.txt", 0, 20, 0, 10, 10, 100, 10, 0, false, false});

        Certificate cert;
        checker.checkOverlappingEntries(entries, cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("Overlapping (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        // Entry 1 at 0, size 30+10+10 = 50
        entries.push_back({"file1.txt", 0, 20, 0, 10, 10, 0, 10, 0, false, false});
        // Entry 2 at 40 (overlaps Entry 1)
        entries.push_back({"file2.txt", 0, 20, 0, 10, 10, 40, 10, 0, false, false});

        Certificate cert;
        checker.checkOverlappingEntries(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "[SECURITY] Overlapping File Entries Check"));
        CHECK(has_error_with_text(cert, "Overlapping file entries"));
    }
}
