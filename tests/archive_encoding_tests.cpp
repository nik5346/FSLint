#include "archive_checker.h"
#include "certificate.h"
#include "test_helpers.h"
#include "zipper.h"
#include <catch2/catch_test_macros.hpp>
#include <vector>

TEST_CASE("Language Encoding Flag Logic", "[archive][encoding]")
{
    ArchiveChecker checker;
    constexpr uint16_t BIT11 = 0x800;

    SECTION("ASCII filename, Bit 11 not set (PASS)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry entry{};
        entry.filename = "test.txt";
        entry.raw_filename = entry.filename;
        entries.push_back(entry);

        Certificate cert;
        checker.checkLanguageEncodingFlag(entries, cert);
        CHECK_FALSE(has_fail(cert));
        CHECK_FALSE(has_warning(cert));
    }

    SECTION("ASCII filename, Bit 11 set (WARNING)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry entry{};
        entry.filename = "test.txt";
        entry.raw_filename = entry.filename;
        entry.flags = BIT11;
        entries.push_back(entry);

        Certificate cert;
        checker.checkLanguageEncodingFlag(entries, cert);
        CHECK_FALSE(has_fail(cert));
        CHECK(has_warning(cert));
        CHECK(has_warning_with_text(cert, "for maximum portability, keeping this bit at 0 is recommended"));
    }

    SECTION("Non-ASCII filename, Bit 11 set (PASS)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry entry{};
        entry.filename = "\xF0\x9F\x9A\x80.txt";
        entry.raw_filename = entry.filename;
        entry.flags = BIT11;
        entries.push_back(entry);

        Certificate cert;
        checker.checkLanguageEncodingFlag(entries, cert);
        CHECK_FALSE(has_fail(cert));
        CHECK_FALSE(has_warning(cert));
    }

    SECTION("Non-ASCII filename, Bit 11 not set (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry entry{};
        entry.filename = "\xF0\x9F\x9A\x80.txt";
        entry.raw_filename = entry.filename;
        entries.push_back(entry);

        Certificate cert;
        checker.checkLanguageEncodingFlag(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "must be set for"));
        CHECK(has_error_with_text(cert, "because it contains non-ASCII characters"));
    }

    SECTION("Mixed filenames (FAIL + WARNING)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry e1{};
        e1.filename = "ascii.txt";
        e1.raw_filename = e1.filename;
        e1.flags = BIT11;
        entries.push_back(e1);

        ZipFileEntry e2{};
        e2.filename = "\xF0\x9F\x9A\x80.txt";
        e2.raw_filename = e2.filename;
        entries.push_back(e2);

        Certificate cert;
        checker.checkLanguageEncodingFlag(entries, cert);
        CHECK(has_fail(cert));
        // Should have both messages
        CHECK(has_warning_with_text(cert, "for maximum portability"));
        CHECK(has_error_with_text(cert, "must be set for"));
    }
}

TEST_CASE("Path Format Non-ASCII Warning Logic", "[archive][encoding]")
{
    ArchiveChecker checker;
    constexpr uint16_t BIT11 = 0x800;

    SECTION("Non-ASCII filename, Bit 11 not set (WARNING)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry entry{};
        entry.filename = "\xF0\x9F\x9A\x80.txt";
        entry.raw_filename = entry.filename;
        entries.push_back(entry);

        Certificate cert;
        checker.checkPathFormat(entries, cert);
        CHECK_FALSE(has_fail(cert));
        CHECK(has_warning(cert));
        CHECK(has_warning_with_text(cert, "Non-ASCII characters in path"));
    }

    SECTION("Non-ASCII filename, Bit 11 set (PASS)")
    {
        std::vector<ZipFileEntry> entries;
        ZipFileEntry entry{};
        entry.filename = "\xF0\x9F\x9A\x80.txt";
        entry.raw_filename = entry.filename;
        entry.flags = BIT11;
        entries.push_back(entry);

        Certificate cert;
        checker.checkPathFormat(entries, cert);
        CHECK_FALSE(has_fail(cert));
        CHECK_FALSE(has_warning(cert));
    }
}
