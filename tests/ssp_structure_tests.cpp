#include "certificate.h"
#include "ssp_directory_checker.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("SSP Directory Validation", "[directory][ssp]")
{
    SspDirectoryChecker checker;

    SECTION("Passing Cases")
    {
        Certificate cert;
        checker.validate("tests/data/ssp/pass/with_doc_entry", cert);
        CHECK_FALSE(has_fail(cert));
        CHECK_FALSE(has_warning(cert));
    }

    SECTION("Warning Cases")
    {
        Certificate cert;
        checker.validate("tests/data/ssp/warn/missing_doc_entry", cert);
        REQUIRE(has_warning(cert));
        CHECK(has_warning_with_text(cert, "Recommended entry point 'documentation/index.html' is missing."));
    }
}
