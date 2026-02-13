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
#include <vector>

namespace fs = std::filesystem;

static void setup_dummy_binaries()
{
    // FMI 2.0
    create_dummy_binary("tests/data/fmi2/pass/dist_binaries_only/binaries/win64/test.dll");
    create_dummy_binary("tests/data/fmi2/pass/structured_naming/binaries/win64/Test.dll");
    create_dummy_binary("tests/data/fmi2/pass/binaries/win64/ValidFMI2.dll");
    create_dummy_binary("tests/data/fmi2/warn/license_entry_missing/binaries/linux64/Test.so");
    create_dummy_binary("tests/data/fmi2/warn/external_dependencies_missing/binaries/linux64/Test.so");
    create_dummy_binary("tests/data/fmi2/warn/empty_dir/binaries/linux64/Test.so");

    fs::create_directories("tests/data/fmi2/warn/license_entry_missing/licenses");
    fs::create_directories("tests/data/fmi2/warn/empty_dir/documentation");

    // FMI 3.0
    create_dummy_binary("tests/data/fmi3/pass/stop_time_inf/binaries/win64/Test.dll");
    create_dummy_binary("tests/data/fmi3/pass/binaries/win64/ValidFMI3.dll");
    create_dummy_binary("tests/data/fmi3/warn/unknown_entry/binaries/x86_64-windows/test.dll");
    create_dummy_binary("tests/data/fmi3/warn/index_html_missing/binaries/x86_64-linux/ValidFMI3.so");
    create_dummy_binary("tests/data/fmi3/warn/no_binary_matching_id/binaries/x86_64-windows/wrong.dll");
    create_dummy_binary("tests/data/fmi3/warn/extra_rdn_invalid/binaries/x86_64-linux/ValidFMI3.so");
    create_dummy_binary("tests/data/fmi3/warn/empty_dir/binaries/x86_64-linux/ValidFMI3.so");
    create_dummy_binary("tests/data/fmi3/warn/unknown_directory/binaries/x86_64-linux/ValidFMI3.so");
    create_dummy_binary("tests/data/fmi3/warn/invalid_tuple/binaries/win64/ValidFMI3.dll");
    create_dummy_binary("tests/data/fmi3/fail/license_entry_missing/binaries/x86_64-linux/ValidFMI3.so");
    create_dummy_binary("tests/data/fmi3/fail/external_dependencies_missing/binaries/x86_64-linux/Test.so");
    create_dummy_binary("tests/data/fmi3/fail/diagram_png_missing/binaries/x86_64-linux/ValidFMI3.so");
    create_dummy_binary("tests/data/fmi3/fail/diagram_png_missing/binaries/x86_64-windows/test.dll");
    create_dummy_binary("tests/data/fmi3/fail/invalid_abi/binaries/x86_64-linux-123/ValidFMI3.so");
    create_dummy_binary("tests/data/fmi3/fail/static_linking_doc_missing/binaries/x86_64-windows-msvc/Test.dll");
    create_dummy_binary("tests/data/fmi3/fail/svg_fallback_missing/binaries/x86_64-linux/ValidFMI3.so");
    create_dummy_binary("tests/data/fmi3/fail/external_dependencies_no_doc/binaries/x86_64-linux/ValidFMI3.so");

    fs::create_directories("tests/data/fmi3/fail/license_entry_missing/documentation/licenses");
    fs::create_directories("tests/data/fmi3/warn/empty_dir/documentation");
    fs::create_directories("tests/data/fmi3/warn/index_html_missing/documentation");
    fs::create_directories("tests/data/fmi3/warn/unknown_directory/unknown_dir");
    fs::create_directories("tests/data/fmi3/warn/extra_rdn_invalid/extra/invalid_rdn");

    // Other
    create_dummy_binary("tests/data/directory/pass/binaries/binaries/win64/binaries.dll");
}

static void cleanup_dummy_binaries()
{
    std::vector<std::string> dirs_to_clean = {
        "tests/data/fmi2/pass/dist_binaries_only/binaries",
        "tests/data/fmi2/pass/structured_naming/binaries",
        "tests/data/fmi2/pass/binaries/binaries",
        "tests/data/fmi2/warn/license_entry_missing/binaries",
        "tests/data/fmi2/warn/license_entry_missing/licenses",
        "tests/data/fmi2/warn/external_dependencies_missing/binaries",
        "tests/data/fmi2/warn/empty_dir/binaries",
        "tests/data/fmi2/warn/empty_dir/documentation",
        "tests/data/fmi3/pass/stop_time_inf/binaries",
        "tests/data/fmi3/pass/binaries/binaries",
        "tests/data/fmi3/warn/unknown_entry/binaries",
        "tests/data/fmi3/warn/index_html_missing/binaries",
        "tests/data/fmi3/warn/index_html_missing/documentation",
        "tests/data/fmi3/warn/no_binary_matching_id/binaries",
        "tests/data/fmi3/warn/extra_rdn_invalid/binaries",
        "tests/data/fmi3/warn/extra_rdn_invalid/extra",
        "tests/data/fmi3/warn/empty_dir/binaries",
        "tests/data/fmi3/warn/empty_dir/documentation",
        "tests/data/fmi3/warn/unknown_directory/binaries",
        "tests/data/fmi3/warn/unknown_directory/unknown_dir",
        "tests/data/fmi3/warn/invalid_tuple/binaries",
        "tests/data/fmi3/fail/license_entry_missing/binaries",
        "tests/data/fmi3/fail/license_entry_missing/documentation",
        "tests/data/fmi3/fail/external_dependencies_missing/binaries",
        "tests/data/fmi3/fail/diagram_png_missing/binaries",
        "tests/data/fmi3/fail/invalid_abi/binaries",
        "tests/data/fmi3/fail/static_linking_doc_missing/binaries",
        "tests/data/fmi3/fail/svg_fallback_missing/binaries",
        "tests/data/fmi3/fail/external_dependencies_no_doc/binaries",
        "tests/data/directory/pass/binaries/binaries"
    };

    for (const auto& dir : dirs_to_clean)
    {
        if (fs::exists(dir))
        {
            fs::remove_all(dir);
        }
    }
}

struct ScopedTestSetup {
    ScopedTestSetup() { setup_dummy_binaries(); }
    ~ScopedTestSetup() { cleanup_dummy_binaries(); }
};

TEST_CASE("FMI 2.0 Directory Validation", "[directory][fmi2]")
{
    ScopedTestSetup setup;
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
                std::string status_str =
                    (res.status == TestStatus::PASS ? "PASS" : (res.status == TestStatus::FAIL ? "FAIL" : "WARN"));
                UNSCOPED_INFO("  " << status_str << ": " << res.test_name);
                for (const auto& msg : res.messages)
                    UNSCOPED_INFO("    - " << msg);
            }
        }
        REQUIRE(has_fail(cert));
        REQUIRE(has_error_with_text(cert, expected_error));
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
                std::string status_str =
                    (res.status == TestStatus::PASS ? "PASS" : (res.status == TestStatus::FAIL ? "FAIL" : "WARN"));
                UNSCOPED_INFO("  " << status_str << ": " << res.test_name);
                for (const auto& msg : res.messages)
                    UNSCOPED_INFO("    - " << msg);
            }
        }
        REQUIRE(has_warning(cert));
        REQUIRE(has_warning_with_text(cert, expected_warning));
    };

    SECTION("Failure Cases")
    {
        validate_fail("tests/data/directory/fail/no_impl", "must contain either a precompiled binary");
    }

    SECTION("Warning Cases")
    {
        validate_warning("tests/data/directory/pass/binaries", "model.png");
        validate_warning("tests/data/directory/pass/sources", "model.png");
        validate_warning("tests/data/directory/warn/unknown_entry", "Unknown file");
        validate_warning("tests/data/fmi2/warn/dist_sources_only", "only contains <SourceFiles>");
        validate_warning("tests/data/fmi2/warn/dist_build_desc_only", "only contains buildDescription.xml");
        validate_warning("tests/data/fmi2/warn/external_dependencies_missing",
                         "needsExecutionTool is true, but 'documentation/externalDependencies.{txt|html}' is missing");
        validate_warning("tests/data/fmi2/warn/license_entry_missing",
                         "licenses/' exists but does not contain a 'license.txt' or 'license.html'");
        validate_warning("tests/data/fmi2/warn/empty_dir", "Standard directory 'documentation' is empty");
    }

    SECTION("Passing Cases")
    {
        validate_pass("tests/data/fmi2/pass/dist_both");
    }
}

TEST_CASE("FMI 3.0 Directory Validation", "[directory][fmi3]")
{
    ScopedTestSetup setup;
    Fmi3DirectoryChecker checker;

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
                std::string status_str =
                    (res.status == TestStatus::PASS ? "PASS" : (res.status == TestStatus::FAIL ? "FAIL" : "WARN"));
                UNSCOPED_INFO("  " << status_str << ": " << res.test_name);
                for (const auto& msg : res.messages)
                    UNSCOPED_INFO("    - " << msg);
            }
        }
        REQUIRE(has_fail(cert));
        REQUIRE(has_error_with_text(cert, expected_error));
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
                std::string status_str =
                    (res.status == TestStatus::PASS ? "PASS" : (res.status == TestStatus::FAIL ? "FAIL" : "WARN"));
                UNSCOPED_INFO("  " << status_str << ": " << res.test_name);
                for (const auto& msg : res.messages)
                    UNSCOPED_INFO("    - " << msg);
            }
        }
        REQUIRE(has_warning(cert));
        REQUIRE(has_warning_with_text(cert, expected_warning));
    };

    SECTION("Failure Cases")
    {
        validate_fail("tests/data/fmi3/fail/external_dependencies_missing", "needsExecutionTool");
        validate_fail("tests/data/fmi3/fail/external_dependencies_no_doc", "needsExecutionTool");
        validate_fail("tests/data/fmi3/fail/license_entry_missing", "license");
        validate_fail("tests/data/fmi3/fail/static_linking_doc_missing", "staticLinking");
        validate_fail("tests/data/fmi3/fail/build_description_missing", "buildDescription.xml");
        validate_fail("tests/data/fmi3/fail/invalid_abi", "ABI name");
        validate_fail("tests/data/fmi3/fail/no_impl", "at least one implementation");
        validate_fail("tests/data/fmi3/fail/diagram_png_missing", "diagram.png");
        validate_fail("tests/data/fmi3/fail/svg_fallback_missing", "fallback");
    }

    SECTION("Warning Cases")
    {
        validate_warning("tests/data/fmi3/warn/unknown_entry", "Unknown file in FMU root");
        validate_warning("tests/data/fmi3/warn/unknown_directory", "Unknown directory in FMU root");
        validate_warning("tests/data/fmi3/warn/index_html_missing", "documentation/index.html' is missing");
        validate_warning("tests/data/fmi3/warn/invalid_tuple", "does not follow the <arch>-<sys>[-<abi>] format");
        validate_warning("tests/data/fmi3/warn/no_binary_matching_id",
                         "does not contain a binary matching any modelIdentifier");
        validate_warning("tests/data/fmi3/warn/extra_rdn_invalid", "should use reverse domain name notation");
        validate_warning("tests/data/fmi3/warn/empty_dir", "Standard directory 'documentation' is empty");
    }

    SECTION("Passing Cases")
    {
        validate_pass("tests/data/fmi3/pass");
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
        REQUIRE(has_warning_with_text(cert, expected_warning));
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
