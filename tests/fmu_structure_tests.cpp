#include "build_description_checker.h"
#include "certificate.h"
#include "fmi2_directory_checker.h"
#include "fmi3_directory_checker.h"
#include "fmi2_model_description_checker.h"
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
        SECTION("diagram.png missing")
        {
            auto path = fs::temp_directory_path() / "fmi3_diag_test";
            fs::remove_all(path);
            fs::create_directories(path / "documentation");
            {
                std::ofstream ofs(path / "modelDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    << "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"test\" instantiationToken=\"{...}\">"
                    << "<CoSimulation modelIdentifier=\"test\"/>"
                    << "</fmiModelDescription>";
            }
            {
                std::ofstream ofs(path / "documentation" / "diagram.svg");
                ofs << "<svg/>";
            }
            fs::create_directories(path / "binaries/x86_64-windows");
            {
                std::ofstream ofs(path / "binaries/x86_64-windows/test.dll");
                ofs << "bin";
            }

            validate_warning(path, "diagram.png is missing");
            fs::remove_all(path);
        }

        SECTION("Unknown entry in root")
        {
            auto path = fs::temp_directory_path() / "fmi3_unknown_entry";
            fs::remove_all(path);
            fs::create_directories(path);
            {
                std::ofstream ofs(path / "modelDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    << "<fmiModelDescription fmiVersion=\"3.0\" modelName=\"test\" instantiationToken=\"{...}\">"
                    << "<CoSimulation modelIdentifier=\"test\"/>"
                    << "</fmiModelDescription>";
            }
            {
                std::ofstream ofs(path / "unknown_file.txt");
                ofs << "test";
            }
            fs::create_directories(path / "binaries/x86_64-windows");
            {
                std::ofstream ofs(path / "binaries/x86_64-windows/test.dll");
                ofs << "bin";
            }

            validate_warning(path, "Unknown entry");
            fs::remove_all(path);
        }
    }
}

TEST_CASE("Build Description Validation", "[build_description]")
{
    BuildDescriptionChecker checker("3.0");

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

        SECTION("version mismatch")
        {
            auto path = fs::temp_directory_path() / "fmi_version_mismatch";
            fs::remove_all(path);
            fs::create_directories(path / "sources");
            {
                std::ofstream ofs(path / "sources/buildDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><fmiBuildDescription fmiVersion=\"2.0\"/>";
            }
            validate_fail(path, "does not match FMU version");
            fs::remove_all(path);
        }

        SECTION("modelIdentifier mismatch")
        {
            auto path = fs::temp_directory_path() / "id_mismatch";
            fs::remove_all(path);
            fs::create_directories(path / "sources");
            {
                std::ofstream ofs(path / "modelDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><fmiModelDescription fmiVersion=\"3.0\" "
                       "modelName=\"test\" instantiationToken=\"{...}\"><CoSimulation "
                       "modelIdentifier=\"real_id\"/></fmiModelDescription>";
            }
            {
                std::ofstream ofs(path / "sources/buildDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><fmiBuildDescription "
                       "fmiVersion=\"3.0\"><BuildConfiguration modelIdentifier=\"wrong_id\"/></fmiBuildDescription>";
            }
            validate_fail(path, "does not match any modelIdentifier");
            fs::remove_all(path);
        }

        SECTION("path contains ..")
        {
            auto path = fs::temp_directory_path() / "dotdot_test";
            fs::remove_all(path);
            fs::create_directories(path / "sources");
            {
                std::ofstream ofs(path / "sources/buildDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><fmiBuildDescription "
                       "fmiVersion=\"3.0\"><BuildConfiguration><SourceFile "
                       "name=\"../secret.c\"/></BuildConfiguration></fmiBuildDescription>";
            }
            validate_fail(path, "contains illegal '..' sequence");
            fs::remove_all(path);
        }
    }

    SECTION("Warning Cases")
    {
        validate_warning("tests/data/build_description/warn/unlisted_file", "is not listed in 'buildDescription.xml'");

        SECTION("Reverse check flags unlisted .C (uppercase)")
        {
            auto path = fs::temp_directory_path() / "bd_ext_C_test";
            fs::remove_all(path);
            fs::create_directories(path / "sources");
            {
                std::ofstream ofs(path / "sources/buildDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><fmiBuildDescription fmiVersion=\"3.0\"/>";
            }
            {
                std::ofstream ofs(path / "sources/File.C");
                ofs << "int x;";
            }
            validate_warning(path, "is not listed in 'buildDescription.xml'");
            fs::remove_all(path);
        }

        SECTION("unknown language")
        {
            auto path = fs::temp_directory_path() / "bd_attr_test_lang";
            fs::remove_all(path);
            fs::create_directories(path / "sources");
            {
                std::ofstream ofs(path / "sources/buildDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><fmiBuildDescription fmiVersion=\"3.0\"><BuildConfiguration modelIdentifier=\"test\" language=\"rust\"/></fmiBuildDescription>";
            }
            validate_warning(path, "is not one of the suggested values");
            fs::remove_all(path);
        }

        SECTION("unknown compiler")
        {
            auto path = fs::temp_directory_path() / "bd_attr_test_comp";
            fs::remove_all(path);
            fs::create_directories(path / "sources");
            {
                std::ofstream ofs(path / "sources/buildDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><fmiBuildDescription fmiVersion=\"3.0\"><BuildConfiguration modelIdentifier=\"test\" compiler=\"mycc\"/></fmiBuildDescription>";
            }
            validate_warning(path, "is not one of the suggested values");
            fs::remove_all(path);
        }
    }

    SECTION("Passing Cases")
    {
        validate_pass("tests/data/build_description/pass/valid");

        SECTION("Accepts C++26 language")
        {
            auto path = fs::temp_directory_path() / "bd_cpp26_test";
            fs::remove_all(path);
            fs::create_directories(path / "sources");
            {
                std::ofstream ofs(path / "sources/buildDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><fmiBuildDescription fmiVersion=\"3.0\"><BuildConfiguration modelIdentifier=\"test\" language=\"C++26\"/></fmiBuildDescription>";
            }
            validate_pass(path);
            fs::remove_all(path);
        }

        SECTION("modelIdentifier matches one of many")
        {
            auto path = fs::temp_directory_path() / "multi_id_match";
            fs::remove_all(path);
            fs::create_directories(path / "sources");
            {
                std::ofstream ofs(path / "modelDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><fmiModelDescription fmiVersion=\"3.0\" "
                       "modelName=\"test\" instantiationToken=\"{...}\"><ModelExchange "
                       "modelIdentifier=\"me_id\"/><CoSimulation modelIdentifier=\"cs_id\"/></fmiModelDescription>";
            }
            {
                std::ofstream ofs(path / "sources/buildDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><fmiBuildDescription "
                       "fmiVersion=\"3.0\"><BuildConfiguration modelIdentifier=\"me_id\"/></fmiBuildDescription>";
            }
            validate_pass(path);
            fs::remove_all(path);
        }
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
        validate_fail("tests/data/fmi2/fail/missing_source", "listed in 'modelDescription.xml' (line 5) does not exist");
    }
}
