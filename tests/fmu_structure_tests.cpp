#include "build_description_checker.h"
#include "certificate.h"
#include "fmi2_directory_checker.h"
#include "fmi2_model_description_checker.h"
#include "fmi3_build_description_checker.h"
#include "fmi3_directory_checker.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

TEST_CASE("FMI 2.0 Directory Validation", "[directory][fmi2]")
{
    Fmi2DirectoryChecker checker;

    auto validate_pass = [&](const std::string& path)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << path);
        CHECK_FALSE(has_fail(cert));
    };

    auto validate_fail = [&](const std::string& path, const std::string& expected_error)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << path);
        if (!has_error_with_text(cert, expected_error))
        {
            UNSCOPED_INFO("Expected error '" << expected_error << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.status == TestStatus::FAIL)
                {
                    UNSCOPED_INFO("  FAIL: " << res.test_name);
                    for (const auto& msg : res.messages)
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        REQUIRE(has_fail(cert));
        CHECK(has_error_with_text(cert, expected_error));
    };

    auto validate_warning = [&](const std::string& path, const std::string& expected_warning)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << path);
        if (!has_warning_with_text(cert, expected_warning))
        {
            UNSCOPED_INFO("Expected warning '" << expected_warning << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.status == TestStatus::WARNING)
                {
                    UNSCOPED_INFO("  WARN: " << res.test_name);
                    for (const auto& msg : res.messages)
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        REQUIRE(has_warning(cert));
        CHECK(has_warning_with_text(cert, expected_warning));
    };

    SECTION("Failure Cases")
    {
        validate_fail("tests/data/directory/fail/no_impl", "must contain either a precompiled binary");
    }

    SECTION("Warning Cases")
    {
        validate_warning("tests/data/directory/pass/binaries", "model.png");
        validate_warning("tests/data/directory/pass/sources", "model.png");
        validate_warning("tests/data/directory/warn/unknown_entry", "Unknown entry");
        validate_warning("tests/data/fmi2/warn/dist_sources_only", "only contains <SourceFiles>");
        validate_warning("tests/data/fmi2/warn/dist_build_desc_only", "only contains buildDescription.xml");
    }

    SECTION("Passing Cases")
    {
        validate_pass("tests/data/fmi2/pass/dist_both");
    }
}

TEST_CASE("FMI 3.0 Directory Validation", "[directory][fmi3]")
{
    Fmi3DirectoryChecker checker;

    auto validate_warning = [&](const fs::path& path, const std::string& expected_warning)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << path);
        if (!has_warning_with_text(cert, expected_warning))
        {
            UNSCOPED_INFO("Expected warning '" << expected_warning << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.status == TestStatus::WARNING)
                {
                    UNSCOPED_INFO("  WARN: " << res.test_name);
                    for (const auto& msg : res.messages)
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        REQUIRE(has_warning(cert));
        CHECK(has_warning_with_text(cert, expected_warning));
    };

    SECTION("Warning Cases")
    {
        validate_warning("tests/data/fmi3/warn/diagram_png_missing", "diagram.png is missing");
        validate_warning("tests/data/fmi3/warn/unknown_entry", "Unknown entry");
    }
}

TEST_CASE("Build Description Validation", "[build_description]")
{
    Fmi3BuildDescriptionChecker checker("3.0");

    auto validate_pass = [&](const fs::path& path)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << path);
        CHECK_FALSE(has_fail(cert));
    };

    auto validate_fail = [&](const fs::path& path, const std::string& expected_error)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << path);
        if (!has_error_with_text(cert, expected_error))
        {
            UNSCOPED_INFO("Expected error '" << expected_error << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.status == TestStatus::FAIL)
                {
                    UNSCOPED_INFO("  FAIL: " << res.test_name);
                    for (const auto& msg : res.messages)
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        REQUIRE(has_fail(cert));
        CHECK(has_error_with_text(cert, expected_error));
    };

    auto validate_warning = [&](const fs::path& path, const std::string& expected_warning)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << path);
        if (!has_warning_with_text(cert, expected_warning))
        {
            UNSCOPED_INFO("Expected warning '" << expected_warning << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.status == TestStatus::WARNING)
                {
                    UNSCOPED_INFO("  WARN: " << res.test_name);
                    for (const auto& msg : res.messages)
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        REQUIRE(has_warning(cert));
        CHECK(has_warning_with_text(cert, expected_warning));
    };

    SECTION("Failure Cases")
    {
        validate_fail("tests/data/build_description/fail/missing_file", "does not exist in 'sources/' directory");
        validate_fail("tests/data/build_description/fail/missing_dir",
                      "does not exist or is not a directory in 'sources/' directory");
        validate_fail("tests/data/build_description/fail/version_mismatch", "does not match FMU version");
        validate_fail("tests/data/build_description/fail/id_mismatch", "does not match any modelIdentifier");
        validate_fail("tests/data/build_description/fail/dotdot", "contains illegal '..' sequence");
    }

    SECTION("Warning Cases")
    {
        validate_warning("tests/data/build_description/warn/unlisted_file", "is not listed in 'buildDescription.xml'");
        validate_warning("tests/data/build_description/warn/unlisted_uppercase_c",
                         "is not listed in 'buildDescription.xml'");
        validate_warning("tests/data/build_description/warn/unknown_language", "is not one of the suggested values");
        validate_warning("tests/data/build_description/warn/unknown_compiler", "is not one of the suggested values");
    }

    SECTION("Passing Cases")
    {
        validate_pass("tests/data/build_description/pass/valid");
        validate_pass("tests/data/build_description/pass/cpp26");
        validate_pass("tests/data/build_description/pass/multi_id_match");
    }
}

TEST_CASE("FMI 2.0 legacy source files validation", "[fmi2][sources]")
{
    Fmi2ModelDescriptionChecker checker;

    auto validate_fail = [&](const std::string& path, const std::string& expected_error)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << path);
        if (!has_error_with_text(cert, expected_error))
        {
            UNSCOPED_INFO("Expected error '" << expected_error << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.status == TestStatus::FAIL)
                {
                    UNSCOPED_INFO("  FAIL: " << res.test_name);
                    for (const auto& msg : res.messages)
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        REQUIRE(has_fail(cert));
        CHECK(has_error_with_text(cert, expected_error));
    };

    SECTION("Failure Cases")
    {
        validate_fail("tests/data/fmi2/fail/missing_source",
                      "listed in 'modelDescription.xml' (line 5) does not exist");
    }
}
