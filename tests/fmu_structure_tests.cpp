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

static bool reference_fmus_available()
{
    static bool available = fs::exists("tests/reference_fmus/BouncingBall_20") &&
                            fs::exists("tests/reference_fmus/BouncingBall_30");
    return available;
}

class TempTestDir {
public:
    TempTestDir(const std::string& name) {
        path = fs::current_path() / ("tests/tmp_" + name);
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempTestDir() {
        fs::remove_all(path);
    }
    fs::path get_path() const { return path; }

    void copy_from(const fs::path& src) {
        fs::copy(src, path, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    }

    void remove(const std::string& rel_path) {
        fs::remove_all(path / rel_path);
    }

    void add_dir(const std::string& rel_path) {
        fs::create_directories(path / rel_path);
    }

    void add_file(const std::string& rel_path, const std::string& content = "") {
        fs::create_directories((path / rel_path).parent_path());
        std::ofstream ofs(path / rel_path);
        ofs << content;
    }

private:
    fs::path path;
};

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
        if (!has_warning(cert)) {
            UNSCOPED_INFO("No warnings at all in certificate for path: " << path);
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
        if (reference_fmus_available()) {
            {
                TempTestDir temp("fmi2_model_png_warning");
                temp.copy_from("tests/reference_fmus/BouncingBall_20");
                temp.remove("model.png");
                validate_warning(temp.get_path().string(), "Recommended file 'model.png' is missing");
            }
            {
                TempTestDir temp("fmi2_license_warning");
                temp.copy_from("tests/reference_fmus/BouncingBall_20");
                temp.add_dir("licenses");
                validate_warning(temp.get_path().string(), "licenses/' exists but does not contain a 'license.txt'");
            }
            {
                TempTestDir temp("fmi2_empty_doc");
                temp.copy_from("tests/reference_fmus/BouncingBall_20");
                temp.remove("documentation");
                temp.add_dir("documentation");
                validate_warning(temp.get_path().string(), "Standard directory 'documentation' is empty");
            }
        }

        validate_warning("tests/data/directory/warn/unknown_entry", "Unknown file");
        validate_warning("tests/data/fmi2/warn/dist_sources_only", "only contains <SourceFiles>");
        validate_warning("tests/data/fmi2/warn/dist_build_desc_only", "only contains buildDescription.xml");

        {
             TempTestDir temp("fmi2_ext_deps_missing");
             temp.add_file("modelDescription.xml", R"(<?xml version="1.0" encoding="UTF-8"?>
<fmiModelDescription fmiVersion="2.0" modelName="test" guid="{1}">
  <CoSimulation modelIdentifier="test" needsExecutionTool="true"/>
</fmiModelDescription>)");
             temp.add_dir("sources");
             temp.add_file("sources/source.c");
             validate_warning(temp.get_path().string(), "needsExecutionTool is true, but 'documentation/externalDependencies.{txt|html}' is missing");
        }
    }

    SECTION("Passing Cases")
    {
        validate_pass("tests/data/fmi2/pass/dist_both");
        if (reference_fmus_available()) {
            validate_pass("tests/reference_fmus/BouncingBall_20");
        }
    }
}

TEST_CASE("FMI 3.0 Directory Validation", "[directory][fmi3]")
{
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
        validate_fail("tests/data/fmi3/fail/no_impl", "at least one implementation");

        if (reference_fmus_available()) {
            {
                TempTestDir temp("fmi3_no_diagram_png");
                temp.copy_from("tests/reference_fmus/BouncingBall_30");
                temp.add_file("documentation/diagram.svg", "<svg></svg>");
                validate_fail(temp.get_path(), "diagram.png is missing");
            }
            {
                TempTestDir temp("fmi3_license_fail");
                temp.copy_from("tests/reference_fmus/BouncingBall_30");
                temp.add_dir("documentation/licenses");
                validate_fail(temp.get_path(), "license");
            }
            {
                TempTestDir temp("fmi3_ext_deps_fail");
                temp.add_file("modelDescription.xml", R"(<fmiModelDescription fmiVersion="3.0" modelName="Test" instantiationToken="1">
  <CoSimulation modelIdentifier="Test" needsExecutionTool="true"/>
</fmiModelDescription>)");
                temp.add_dir("sources");
                temp.add_file("sources/buildDescription.xml", R"(<buildDescription fmiVersion="3.0"><BuildConfiguration modelIdentifier="Test"/></buildDescription>)");
                validate_fail(temp.get_path(), "needsExecutionTool is true");
            }
        }
    }

    SECTION("Warning Cases")
    {
        validate_warning("tests/data/fmi3/warn/unknown_entry", "Unknown file in FMU root");

        if (reference_fmus_available()) {
            {
                TempTestDir temp("fmi3_invalid_tuple");
                temp.copy_from("tests/reference_fmus/BouncingBall_30");
                // Create an invalid tuple directory (no dashes)
                temp.add_dir("binaries/invalid_tuple");
                validate_warning(temp.get_path(), "does not follow the <arch>-<sys>");
            }
            {
                TempTestDir temp("fmi3_rdn_warning");
                temp.copy_from("tests/reference_fmus/BouncingBall_30");
                temp.add_dir("extra/not_rdn");
                validate_warning(temp.get_path(), "should use reverse domain name notation");
            }
            {
                TempTestDir temp("fmi3_index_html_warning");
                temp.copy_from("tests/reference_fmus/BouncingBall_30");
                temp.remove("documentation/index.html");
                validate_warning(temp.get_path(), "documentation/index.html' is missing");
            }
        }
    }

    SECTION("Passing Cases")
    {
        if (reference_fmus_available()) {
            validate_pass("tests/reference_fmus/BouncingBall_30");
        }
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
