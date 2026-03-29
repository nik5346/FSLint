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
        entries.push_back({"test.txt", 0, 20, 0, 0, 0, false, false});

        Certificate cert;
        checker.checkLanguageEncodingFlag(entries, cert);
        CHECK_FALSE(has_fail(cert));
        CHECK_FALSE(has_warning(cert));
    }

    SECTION("ASCII filename, Bit 11 set (WARNING)")
    {
        std::vector<ZipFileEntry> entries;
        entries.push_back({"test.txt", 0, 20, BIT11, 0, 0, false, false});

        Certificate cert;
        checker.checkLanguageEncodingFlag(entries, cert);
        CHECK_FALSE(has_fail(cert));
        CHECK(has_warning(cert));
        CHECK(has_warning_with_text(cert, "for maximum portability, keeping this bit at 0 is recommended"));
    }

    SECTION("Non-ASCII filename, Bit 11 set (PASS)")
    {
        std::vector<ZipFileEntry> entries;
        entries.push_back({"\xF0\x9F\x9A\x80.txt", 0, 20, BIT11, 0, 0, false, false}); // Rocket emoji

        Certificate cert;
        checker.checkLanguageEncodingFlag(entries, cert);
        CHECK_FALSE(has_fail(cert));
        CHECK_FALSE(has_warning(cert));
    }

    SECTION("Non-ASCII filename, Bit 11 not set (FAIL)")
    {
        std::vector<ZipFileEntry> entries;
        entries.push_back({"\xF0\x9F\x9A\x80.txt", 0, 20, 0, 0, 0, false, false}); // Rocket emoji

        Certificate cert;
        checker.checkLanguageEncodingFlag(entries, cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "must be set for"));
        CHECK(has_error_with_text(cert, "because it contains non-ASCII characters"));
    }

    SECTION("Mixed filenames (FAIL + WARNING)")
    {
        std::vector<ZipFileEntry> entries;
        entries.push_back({"ascii.txt", 0, 20, BIT11, 0, 0, false, false});
        entries.push_back({"\xF0\x9F\x9A\x80.txt", 0, 20, 0, 0, 0, false, false});

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
        // Using a filename with non-ASCII and no bit 11
        entries.push_back({"\xF0\x9F\x9A\x80.txt", 0, 20, 0, 0, 0, false, false});

        Certificate cert;
        checker.checkPathFormat(entries, cert);
        CHECK_FALSE(has_fail(cert));
        CHECK(has_warning(cert));
        CHECK(has_warning_with_text(cert, "Non-ASCII characters in path"));
    }

    SECTION("Non-ASCII filename, Bit 11 set (PASS)")
    {
        std::vector<ZipFileEntry> entries;
        // Using a filename with non-ASCII and bit 11
        entries.push_back({"\xF0\x9F\x9A\x80.txt", 0, 20, BIT11, 0, 0, false, false});

        Certificate cert;
        checker.checkPathFormat(entries, cert);
        CHECK_FALSE(has_fail(cert));
        CHECK_FALSE(has_warning(cert));
    }
}
