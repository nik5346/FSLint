#include "build_description_checker.h"
#include "certificate.h"
#include "checker_factory.h"
#include "file_utils.h"
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

    SECTION("Passing Cases")
    {
        Certificate cert;
        Fmi1DirectoryChecker pass_checker;
        pass_checker.setOriginalPath("TestME.fmu");
        pass_checker.validate("tests/data/fmi1/pass/clean_me", cert);
        if (has_warning(cert))
        {
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::WARNING)
                {
                    std::cout << "WARN in FMI 1.0 clean_me: " << res.getName() << std::endl;
                    for (const auto& msg : res.getMessages())
                        std::cout << "  - " << msg << std::endl;
                }
            }
        }
        CHECK_FALSE(has_fail(cert));
        CHECK_FALSE(has_warning(cert));

        if (reference_fmus_available())
        {
            auto validate_ref_fmu = [&](const fs::path& path)
            {
                Certificate cert;
                ModelChecker mc;
                cert = mc.validate(path, true);
                INFO("Checking path: " << file_utils::pathToUtf8(path));
                // Reference FMUs for FMI 1.0 are missing _main.html, model.png and documentation/licenses/
                // We check that there are no FAILs other than the expected ones if any
                for (const auto& res : cert.getResults())
                {
                    if (res.getStatus() == TestStatus::FAIL)
                    {
                        for (const auto& msg : res.getMessages())
                            if (msg.find("_main.html") == std::string::npos &&
                                msg.find("documentation/licenses") == std::string::npos)
                                FAIL("Unexpected failure in reference FMU " << file_utils::pathToUtf8(path) << ": "
                                                                            << msg);
                    }
                }
            };
            validate_ref_fmu("tests/reference_fmus/1.0/cs/BouncingBall.fmu");
            validate_ref_fmu("tests/reference_fmus/1.0/me/BouncingBall.fmu");
        }
    }

    SECTION("Model Identifier Mismatch")
    {
        Certificate cert;
        Fmi1DirectoryChecker mismatch_checker;
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

        Certificate cert3;
        checker.validate("tests/data/fmi1/fail/missing_binary", cert3);
        REQUIRE(has_fail(cert3));
        CHECK(has_error_with_text(cert3, "does not contain a binary matching modelIdentifier"));
    }

    SECTION("Warning Cases")
    {
        auto validate_warning = [&](const fs::path& path, const std::string& expected_warning,
                                    const std::string& original_path = "Test.fmu")
        {
            Certificate cert;
            Fmi1DirectoryChecker warn_checker;
            warn_checker.setOriginalPath(original_path);
            warn_checker.validate(path, cert);
            INFO("Checking path: " << file_utils::pathToUtf8(path));
            if (!has_warning_with_text(cert, expected_warning))
            {
                UNSCOPED_INFO("Expected warning '" << expected_warning << "' not found in results:");
                for (const auto& res : cert.getResults())
                {
                    if (res.getStatus() == TestStatus::WARNING)
                    {
                        UNSCOPED_INFO("  WARN: " << res.getName());
                        for (const auto& msg : res.getMessages())
                            UNSCOPED_INFO("    - " << msg);
                    }
                }
            }
            REQUIRE(has_warning(cert));
            CHECK(has_warning_with_text(cert, expected_warning));
        };

        {
            Certificate cert;
            checker.validate("tests/data/fmi1/warn/missing_main_html", cert);
            CHECK(has_error_with_text(cert, "The documentation entry point 'documentation/_main.html' is missing."));
        }

        validate_warning("tests/data/directory/warn/missing_doc_entry", "Providing documentation is recommended in 'documentation/'.");
        validate_warning("tests/data/fmi1/warn/fmi_headers_in_sources",
                         "Standard FMI header file 'fmiFunctions.h' found in 'sources/' directory");
        validate_warning("tests/data/fmi1/warn/unknown_root_entry", "Unknown file in FMU root: 'unknown.txt'");
        validate_warning("tests/data/fmi1/pass/TestME", "Recommended file 'model.png' is missing", "TestME.fmu");
        validate_warning("tests/data/fmi1/warn/empty_resources", "Standard directory 'resources' is empty");
    }

    SECTION("Effectively Empty (Hidden Files)")
    {
        Fmi1DirectoryChecker effectively_empty_checker;
        effectively_empty_checker.setOriginalPath("TestME.fmu");

        Certificate cert;
        effectively_empty_checker.validate("tests/data/fmi1/effectively_empty_ds_store", cert);
        CHECK(has_warning_with_text(cert, "Standard directory 'resources' is empty."));

        Certificate cert2;
        effectively_empty_checker.validate("tests/data/fmi1/effectively_empty_thumbs_db", cert2);
        CHECK(has_warning_with_text(cert2, "Standard directory 'resources' is empty."));
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
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (has_fail(cert))
        {
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::FAIL)
                {
                    UNSCOPED_INFO("  FAIL: " << res.getName());
                    for (const auto& msg : res.getMessages())
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        CHECK_FALSE(has_fail(cert));
    };

    auto validate_clean_pass = [&](const fs::path& path)
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
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (has_fail(cert) || has_warning(cert))
        {
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::FAIL || res.getStatus() == TestStatus::WARNING)
                {
                    std::string status_str = (res.getStatus() == TestStatus::FAIL ? "FAIL" : "WARN");
                    UNSCOPED_INFO("  " << status_str << ": " << res.getName());
                    for (const auto& msg : res.getMessages())
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        if (has_warning(cert))
        {
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::WARNING)
                {
                    std::cout << "WARN in " << file_utils::pathToUtf8(path) << ": " << res.getName() << std::endl;
                    for (const auto& msg : res.getMessages())
                        std::cout << "  - " << msg << std::endl;
                }
            }
        }
        CHECK_FALSE(has_fail(cert));
        CHECK_FALSE(has_warning(cert));
    };

    auto validate_fail = [&](const std::string& path, const std::string& expected_error)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (!has_error_with_text(cert, expected_error))
        {
            UNSCOPED_INFO("Expected error '" << expected_error << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                std::string status_str =
                    (res.getStatus() == TestStatus::PASS ? "PASS"
                                                         : (res.getStatus() == TestStatus::FAIL ? "FAIL" : "WARN"));
                UNSCOPED_INFO("  " << status_str << ": " << res.getName());
                for (const auto& msg : res.getMessages())
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
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (!has_warning_with_text(cert, expected_warning))
        {
            UNSCOPED_INFO("Expected warning '" << expected_warning << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                std::string status_str =
                    (res.getStatus() == TestStatus::PASS ? "PASS"
                                                         : (res.getStatus() == TestStatus::FAIL ? "FAIL" : "WARN"));
                UNSCOPED_INFO("  " << status_str << ": " << res.getName());
                for (const auto& msg : res.getMessages())
                    UNSCOPED_INFO("    - " << msg);
            }
        }
        if (!has_warning(cert))
            UNSCOPED_INFO("No warnings at all in certificate for path: " << file_utils::pathToUtf8(path));
        REQUIRE(has_warning(cert));
        REQUIRE(has_warning_with_text(cert, expected_warning));
    };

    SECTION("Failure Cases")
    {
        validate_fail("tests/data/directory/fail/no_impl", "must contain either a precompiled binary");
        validate_fail("tests/data/fmi2/warn/empty_documentation",
                      "The documentation entry point 'documentation/index.html' is missing.");
        validate_fail("tests/data/fmi2/warn/missing_license_txt",
                      "Standard directory 'documentation/licenses' is empty.");
        validate_fail("tests/data/fmi2/fail/undeclared_sources",
                      "Source code FMU contains a 'sources/' directory, but no <SourceFiles> are listed in "
                      "'modelDescription.xml'.");
    }

    SECTION("Warning Cases")
    {
        validate_warning("tests/data/fmi2/warn/missing_model_png", "Recommended file 'model.png' is missing");
        validate_warning("tests/data/directory/warn/missing_doc_entry", "Providing documentation is recommended in 'documentation/'.");
        validate_warning("tests/data/directory/warn/nonstandard_platform_fmi2",
                         "is not one of the standardized FMI 2.0 platform names");
        validate_warning("tests/data/fmi2/warn/empty_extra", "Standard directory 'extra' is empty");
        validate_warning("tests/data/fmi2/warn/not_rdn_extra", "should use reverse domain name notation");

        SECTION("RDNN in extra/ for FMI 2.0 (Success cases)")
        {
            Certificate cert;
            checker.validate("tests/data/fmi2/pass/rdn_extra", cert);
            CHECK_FALSE(has_warning_with_text(cert, "should use reverse domain name notation"));
        }

        validate_warning("tests/data/fmi2/warn/empty_terminalsAndIcons",
                         "Standard directory 'terminalsAndIcons' is empty");
        validate_warning("tests/data/fmi2/warn/empty_licenses_subdir",
                         "Standard directory 'documentation/licenses' is empty");
        validate_warning("tests/data/fmi2/warn/missing_ext_deps",
                         "Since needsExecutionTool='true', 'documentation/externalDependencies.{txt|html}' "
                         "should be present to document the external resources the FMU depends on.");

        validate_warning("tests/data/directory/warn/unknown_entry", "Unknown file");
        validate_warning("tests/data/fmi2/warn/dist_sources_only", "buildDescription.xml' is recommended");
        validate_warning("tests/data/fmi2/warn/dist_build_desc_only", "recommended to also provide <SourceFiles>");
        validate_warning("tests/data/fmi2/warn/fmi_header_in_sources",
                         "Standard FMI header file 'fmi2Functions.h' found in 'sources/' directory");
    }

    SECTION("Passing Cases")
    {
        validate_clean_pass("tests/data/fmi2/pass/clean_dist_both");
        if (reference_fmus_available())
            validate_pass("tests/reference_fmus/2.0/BouncingBall.fmu");
    }

    SECTION("Binary Existence")
    {
        validate_fail("tests/data/directory/fail/missing_binary", "does not contain a binary matching modelIdentifier");
        validate_fail("tests/data/directory/fail/missing_one_binary",
                      "does not contain a binary matching modelIdentifier 'TestME'");
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
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (has_fail(cert))
        {
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::FAIL)
                {
                    UNSCOPED_INFO("  FAIL: " << res.getName());
                    for (const auto& msg : res.getMessages())
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        CHECK_FALSE(has_fail(cert));
    };

    auto validate_clean_pass = [&](const fs::path& path)
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
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (has_fail(cert) || has_warning(cert))
        {
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::FAIL || res.getStatus() == TestStatus::WARNING)
                {
                    std::string status_str = (res.getStatus() == TestStatus::FAIL ? "FAIL" : "WARN");
                    UNSCOPED_INFO("  " << status_str << ": " << res.getName());
                    for (const auto& msg : res.getMessages())
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        CHECK_FALSE(has_fail(cert));
        CHECK_FALSE(has_warning(cert));
    };

    auto validate_fail = [&](const fs::path& path, const std::string& expected_error)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (!has_error_with_text(cert, expected_error))
        {
            UNSCOPED_INFO("Expected error '" << expected_error << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                std::string status_str =
                    (res.getStatus() == TestStatus::PASS ? "PASS"
                                                         : (res.getStatus() == TestStatus::FAIL ? "FAIL" : "WARN"));
                UNSCOPED_INFO("  " << status_str << ": " << res.getName());
                for (const auto& msg : res.getMessages())
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
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (!has_warning_with_text(cert, expected_warning))
        {
            UNSCOPED_INFO("Expected warning '" << expected_warning << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                std::string status_str =
                    (res.getStatus() == TestStatus::PASS ? "PASS"
                                                         : (res.getStatus() == TestStatus::FAIL ? "FAIL" : "WARN"));
                UNSCOPED_INFO("  " << status_str << ": " << res.getName());
                for (const auto& msg : res.getMessages())
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
        validate_fail("tests/data/fmi3/fail/missing_license", "Standard directory 'documentation/licenses' is empty.");
        validate_fail("tests/data/fmi3/fail/missing_ext_deps",
                      "Since needsExecutionTool='true', 'documentation/externalDependencies.{txt|html}' "
                      "must be present to document the external resources the FMU depends on.");
        validate_fail("tests/data/fmi3/fail/missing_icon_png", "fallback");
        validate_fail("tests/data/fmi3/warn/missing_index_html",
                      "The documentation entry point 'documentation/index.html' is missing.");
        validate_fail("tests/data/fmi3/warn/invalid_binaries_tuple",
                      "does not contain a binary matching modelIdentifier");
    }

    SECTION("Warning Cases")
    {
        validate_warning("tests/data/fmi3/warn/unknown_entry", "Unknown file in FMU root");
        validate_warning("tests/data/directory/warn/missing_doc_entry", "Providing documentation is recommended in 'documentation/'.");
        validate_warning("tests/data/directory/warn/nonstandard_platform_fmi3",
                         "is not one of the standardized FMI 3.0 architectures");
        validate_warning("tests/data/fmi3/warn/not_rdn_extra", "should use reverse domain name notation");

        SECTION("RDNN in extra/ for FMI 3.0 (Success cases)")
        {
            Certificate cert;
            checker.validate("tests/data/fmi3/pass/rdn_extra", cert);
            CHECK_FALSE(has_warning_with_text(cert, "should use reverse domain name notation"));
        }

        validate_warning("tests/data/fmi3/warn/missing_icon_png",
                         "Recommended file 'terminalsAndIcons/icon.png' is missing");
        validate_warning("tests/data/fmi3/warn/empty_extra", "Standard directory 'extra' is empty");
        validate_warning("tests/data/fmi3/warn/empty_licenses_subdir",
                         "Standard directory 'documentation/licenses' is empty");
    }

    SECTION("Passing Cases")
    {
        validate_clean_pass("tests/data/fmi3/pass/clean_BouncingBall");
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
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (has_fail(cert))
        {
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::FAIL)
                {
                    UNSCOPED_INFO("  FAIL: " << res.getName());
                    for (const auto& msg : res.getMessages())
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        CHECK_FALSE(has_fail(cert));
    };

    auto validate_fail = [&](const fs::path& path, const std::string& expected_error)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (!has_error_with_text(cert, expected_error))
        {
            UNSCOPED_INFO("Expected error '" << expected_error << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::FAIL)
                {
                    UNSCOPED_INFO("  FAIL: " << res.getName());
                    for (const auto& msg : res.getMessages())
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
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (!has_warning_with_text(cert, expected_warning))
        {
            UNSCOPED_INFO("Expected warning '" << expected_warning << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::WARNING)
                {
                    UNSCOPED_INFO("  WARN: " << res.getName());
                    for (const auto& msg : res.getMessages())
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
        validate_fail("tests/data/build_description/fail/absolute_path_source", "must be a relative path");
        validate_fail("tests/data/build_description/fail/absolute_path_include", "must be a relative path");
        validate_fail("tests/data/build_description/fail/absolute_path_library", "must be a relative path");
        validate_fail("tests/data/build_description/fail/preprocessor_missing_name",
                      "missing required 'name' attribute");
        validate_fail("tests/data/build_description/fail/preprocessor_invalid_name",
                      "is not a valid C preprocessor identifier");
        validate_fail("tests/data/build_description/fail/library_missing_name",
                      "missing required 'name' attribute or it is empty");
    }

    SECTION("Warning Cases")
    {
        validate_warning("tests/data/build_description/warn/unlisted_file", "is not listed in 'buildDescription.xml'");
        validate_warning("tests/data/build_description/warn/unlisted_uppercase_c",
                         "is not listed in 'buildDescription.xml'");
        validate_warning("tests/data/build_description/warn/unknown_language", "is not one of the suggested values");
        validate_warning("tests/data/build_description/warn/unknown_compiler", "is not one of the suggested values");
        validate_warning("tests/data/build_description/warn/preprocessor_optional_no_options",
                         "marked optional but has no <Option> children");
        validate_warning("tests/data/build_description/warn/library_internal_missing",
                         "declared as internal but no matching file was found");
        validate_warning("tests/data/build_description/warn/empty_source_file_set", "contains no SourceFile entries");
        validate_warning("tests/data/build_description/warn/compiler_options_no_compiler",
                         "specifies 'compilerOptions' but no 'compiler' attribute");
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
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (has_fail(cert))
        {
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::FAIL)
                {
                    UNSCOPED_INFO("  FAIL: " << res.getName());
                    for (const auto& msg : res.getMessages())
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        CHECK_FALSE(has_fail(cert));
    };

    auto validate_fail = [&](const fs::path& path, const std::string& expected_error)
    {
        Certificate cert;
        checker.validate(path, cert);
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (!has_error_with_text(cert, expected_error))
        {
            UNSCOPED_INFO("Expected error '" << expected_error << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::FAIL)
                {
                    UNSCOPED_INFO("  FAIL: " << res.getName());
                    for (const auto& msg : res.getMessages())
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
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (!has_error_with_text(cert, expected_error))
        {
            UNSCOPED_INFO("Expected error '" << expected_error << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::FAIL)
                {
                    UNSCOPED_INFO("  FAIL: " << res.getName());
                    for (const auto& msg : res.getMessages())
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
