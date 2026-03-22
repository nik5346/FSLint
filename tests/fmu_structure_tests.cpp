#include "build_description_checker.h"
#include "certificate.h"
#include "checker_factory.h"
#include "fmi1_binary_checker.h"
#include "fmi1_directory_checker.h"
#include "fmi1_model_description_checker.h"
#include "fmi2_build_description_checker.h"
#include "fmi2_directory_checker.h"
#include "fmi2_model_description_checker.h"
#include "fmi3_build_description_checker.h"
#include "fmi3_directory_checker.h"
#include "model_checker.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static bool reference_fmus_available()
{
    static bool available = fs::exists("tests/reference_fmus/1.0/cs/BouncingBall.fmu") &&
                            fs::exists("tests/reference_fmus/1.0/me/BouncingBall.fmu") &&
                            fs::exists("tests/reference_fmus/2.0/BouncingBall.fmu") &&
                            fs::exists("tests/reference_fmus/3.0/BouncingBall.fmu");
    return available;
}

TEST_CASE("FMI 1.0 Directory Validation", "[directory][fmi1]")
{
    Fmi1DirectoryChecker checker;

    auto validate_pass = [&](const fs::path& path)
    {
        Certificate cert;
        if (fs::is_regular_file(path))
        {
            ModelChecker mc;
            cert = mc.validate(path, true);
        }
        else
        {
            checker.validate(path, cert);
        }
        INFO("Checking path: " << path);
        CHECK_FALSE(has_fail(cert));
    };

    SECTION("Passing Cases")
    {
        validate_pass("tests/data/fmi1/pass/TestME");
        validate_pass("tests/data/fmi1/pass/TestCS");
        if (reference_fmus_available())
        {
            validate_pass("tests/reference_fmus/1.0/cs/BouncingBall.fmu");
            validate_pass("tests/reference_fmus/1.0/me/BouncingBall.fmu");
        }
    }

    SECTION("Model Identifier Mismatch")
    {
        Certificate cert;
        Fmi1ModelDescriptionChecker mismatch_checker;
        mismatch_checker.setOriginalPath("WrongName.fmu");
        mismatch_checker.validate("tests/data/fmi1/pass/TestME", cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "must match the FMU filename 'WrongName'"));
    }

    SECTION("Failure Cases")
    {
        Certificate cert;
        checker.validate("tests/data/fmi1/fail/no_impl", cert);
        REQUIRE(has_fail(cert));
        CHECK(has_error_with_text(cert, "must contain either a precompiled binary"));

        Certificate cert2;
        checker.validate("tests/data/fmi1/warn/empty_documentation", cert2);
        REQUIRE(has_fail(cert2));
        CHECK(has_error_with_text(cert2, "The documentation entry point 'documentation/_main.html' is missing."));
    }

    SECTION("Warning Cases")
    {
        auto validate_warning = [&](const fs::path& path, const std::string& expected_warning)
        {
            Certificate cert;
            checker.validate(path, cert);
            INFO("Checking path: " << path);
            REQUIRE(has_warning(cert));
            CHECK(has_warning_with_text(cert, expected_warning));
        };

        {
            Certificate cert;
            checker.validate("tests/data/fmi1/warn/missing_main_html", cert);
            CHECK(has_error_with_text(cert, "The documentation entry point 'documentation/_main.html' is missing."));
        }
        validate_warning("tests/data/directory/warn/missing_doc_entry",
                         "Providing documentation is recommended.");
        validate_warning("tests/data/fmi1/warn/fmi_headers_in_sources",
                         "Standard FMI header file 'fmiFunctions.h' found in 'sources/' directory");
        validate_warning("tests/data/fmi1/warn/unknown_root_entry", "Unknown file in FMU root: 'unknown.txt'");
        validate_warning("tests/data/fmi1/pass/TestME", "Recommended file 'model.png' is missing");
        validate_warning("tests/data/fmi1/warn/empty_resources", "Standard directory 'resources' is empty");
    }
}

TEST_CASE("FMI 2.0 Directory Validation", "[directory][fmi2]")
{
    Fmi2DirectoryChecker checker;

    auto validate_pass = [&](const fs::path& path)
    {
        Certificate cert;
        if (fs::is_regular_file(path))
        {
            ModelChecker mc;
            cert = mc.validate(path, true);
        }
        else
        {
            checker.validate(path, cert);
        }
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
        if (!has_warning(cert))
            UNSCOPED_INFO("No warnings at all in certificate for path: " << path);
        REQUIRE(has_warning(cert));
        REQUIRE(has_warning_with_text(cert, expected_warning));
    };

    SECTION("Failure Cases")
    {
        validate_fail("tests/data/directory/fail/no_impl", "must contain either a precompiled binary");
        validate_fail("tests/data/fmi2/warn/empty_documentation", "The documentation entry point 'documentation/index.html' is missing.");
        validate_fail("tests/data/fmi2/warn/missing_license_txt",
                      "The license entry point (e.g. 'licenses/license.txt') is missing.");
    }

    SECTION("Warning Cases")
    {
        validate_warning("tests/data/fmi2/warn/missing_model_png", "Recommended file 'model.png' is missing");
        validate_warning("tests/data/directory/warn/missing_doc_entry",
                         "Providing documentation is recommended.");
        validate_warning("tests/data/fmi2/warn/empty_extra", "Standard directory 'extra' is empty");
        validate_warning("tests/data/fmi2/warn/empty_terminalsAndIcons",
                         "Standard directory 'terminalsAndIcons' is empty");
        validate_warning("tests/data/fmi2/warn/empty_licenses_subdir",
                         "Standard directory 'documentation/licenses' is empty");
        validate_warning("tests/data/fmi2/warn/missing_ext_deps",
                         "needsExecutionTool is true, but 'documentation/externalDependencies.{txt|html}' is missing");

        validate_warning("tests/data/directory/warn/unknown_entry", "Unknown file");
        validate_warning("tests/data/fmi2/warn/dist_sources_only", "only contains <SourceFiles>");
        validate_warning("tests/data/fmi2/warn/dist_build_desc_only", "only contains buildDescription.xml");
        validate_warning("tests/data/fmi2/warn/fmi_header_in_sources",
                         "Standard FMI header file 'fmi2Functions.h' found in 'sources/' directory");
    }

    SECTION("Passing Cases")
    {
        validate_pass("tests/data/fmi2/pass/dist_both");
        if (reference_fmus_available())
            validate_pass("tests/reference_fmus/2.0/BouncingBall.fmu");
    }
}

TEST_CASE("FMI 3.0 Directory Validation", "[directory][fmi3]")
{
    Fmi3DirectoryChecker checker;

    auto validate_pass = [&](const fs::path& path)
    {
        Certificate cert;
        if (fs::is_regular_file(path))
        {
            ModelChecker mc;
            cert = mc.validate(path, true);
        }
        else
        {
            checker.validate(path, cert);
        }
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
        validate_fail("tests/data/fmi3/fail/no_impl", "FMU must contain at least one implementation");
        validate_fail("tests/data/fmi3/fail/missing_diagram_png", "diagram.png is missing");
        validate_fail("tests/data/fmi3/fail/missing_license", "The license entry point");
        validate_fail("tests/data/fmi3/fail/missing_ext_deps", "externalDependencies");
        validate_fail("tests/data/fmi3/fail/missing_icon_png", "fallback");
        validate_fail("tests/data/fmi3/warn/missing_index_html", "The documentation entry point 'documentation/index.html' is missing.");
    }

    SECTION("Warning Cases")
    {
        validate_warning("tests/data/fmi3/warn/unknown_entry", "Unknown file in FMU root");
        validate_warning("tests/data/directory/warn/missing_doc_entry",
                         "Providing documentation is recommended.");
        validate_warning("tests/data/fmi3/warn/invalid_binaries_tuple", "does not follow the <arch>-<sys>");
        validate_warning("tests/data/fmi3/warn/not_rdn_extra", "should use reverse domain name notation");
        validate_warning("tests/data/fmi3/warn/missing_icon_png",
                         "Recommended file 'terminalsAndIcons/icon.png' is missing");
        validate_warning("tests/data/fmi3/warn/empty_extra", "Standard directory 'extra' is empty");
        validate_warning("tests/data/fmi3/warn/empty_licenses_subdir",
                         "Standard directory 'documentation/licenses' is empty");
    }

    SECTION("Passing Cases")
    {
        if (reference_fmus_available())
            validate_pass("tests/reference_fmus/3.0/BouncingBall.fmu");
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
        validate_fail("tests/data/build_description/fail/dotdot", "contains illegal '.' sequence");
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

    SECTION("FMI 3.0 Version Mismatch")
    {
        Fmi3BuildDescriptionChecker mismatch_checker("3.0.1");
        Certificate cert;
        mismatch_checker.validate("tests/data/fmi3/fail/build_description_mismatch", cert);
        REQUIRE(has_fail(cert));
        CHECK(has_error_with_text(cert, "does not match FMU version (3.0.1)"));
    }
}

TEST_CASE("FMI 2.0 Build Description Validation", "[build_description][fmi2]")
{
    Fmi2BuildDescriptionChecker checker("2.0");

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

    SECTION("Passing Cases")
    {
        validate_pass("tests/data/fmi2/pass/build_description");
    }

    SECTION("Failure Cases")
    {
        validate_fail("tests/data/fmi2/fail/build_description_v2", "must be '3.0' for FMI 2.x FMUs");
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
