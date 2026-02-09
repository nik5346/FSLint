#include "build_description_checker.h"
#include "certificate.h"
#include "checker_factory.h"
#include "fmi2_directory_checker.h"
#include "fmi3_directory_checker.h"
#include "fmi2_model_description_checker.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

bool has_test_with_status(const Certificate& cert, const std::string& test_name, TestStatus status)
{
    const auto& results = cert.getResults();
    for (const auto& r : results)
    {
        if (r.test_name == test_name && r.status == status)
            return true;
    }
    return false;
}

bool has_fail(const Certificate& cert, const std::string& test_name)
{
    return has_test_with_status(cert, test_name, TestStatus::FAIL);
}

bool has_warn(const Certificate& cert, const std::string& test_name)
{
    return has_test_with_status(cert, test_name, TestStatus::WARNING);
}

bool has_pass(const Certificate& cert, const std::string& test_name)
{
    return has_test_with_status(cert, test_name, TestStatus::PASS);
}

TEST_CASE("DirectoryChecker validation", "[directory]")
{
    SECTION("FMI 2.0 distribution")
    {
        Fmi2DirectoryChecker checker;

        SECTION("Fails if neither binaries nor sources")
        {
            Certificate cert;
            checker.validate("tests/data/directory/fail/no_impl", cert);
            CHECK(has_fail(cert, "FMU Structure"));
        }

        SECTION("Passes with binaries (with warning for missing model.png)")
        {
            Certificate cert;
            checker.validate("tests/data/directory/pass/binaries", cert);
            CHECK(has_warn(cert, "FMU Structure"));
        }

        SECTION("Passes with sources (with warnings)")
        {
            Certificate cert;
            checker.validate("tests/data/directory/pass/sources", cert);
            CHECK(has_warn(cert, "FMU Structure"));
        }

        SECTION("Warns about unknown entry in root")
        {
            Certificate cert;
            checker.validate("tests/data/directory/warn/unknown_entry", cert);
            CHECK(has_warn(cert, "FMU Structure"));
        }

        SECTION("Compatibility warnings for sources")
        {
            SECTION("Warns if only SourceFiles is present in MD")
            {
                Certificate cert;
                checker.validate("tests/data/fmi2/warn/dist_sources_only", cert);
                CHECK(has_warn(cert, "FMU Structure"));
            }

            SECTION("Warns if only buildDescription.xml is present")
            {
                Certificate cert;
                checker.validate("tests/data/fmi2/warn/dist_build_desc_only", cert);
                CHECK(has_warn(cert, "FMU Structure"));
            }

            SECTION("Passes if both are present (still warns about model.png)")
            {
                Certificate cert;
                checker.validate("tests/data/fmi2/pass/dist_both", cert);
                CHECK(has_warn(cert, "FMU Structure"));
            }
        }
    }

    SECTION("FMI 3.0 distribution")
    {
        Fmi3DirectoryChecker checker;

        SECTION("Warns if diagram.svg is present but diagram.png is missing")
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

            Certificate cert;
            checker.validate(path, cert);
            CHECK(has_warn(cert, "FMU Structure"));
            fs::remove_all(path);
        }

        SECTION("Warns about unknown entry in root")
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

            Certificate cert;
            checker.validate(path, cert);
            CHECK(has_warn(cert, "FMU Structure"));
            fs::remove_all(path);
        }
    }
}

TEST_CASE("BuildDescriptionChecker validation", "[build_description]")
{
    SECTION("FMI 3.0")
    {
        BuildDescriptionChecker checker("3.0");

        SECTION("Fails if listed source file is missing")
        {
            Certificate cert;
            checker.validate("tests/data/build_description/fail/missing_file", cert);
            CHECK(has_fail(cert, "Build Description Semantic Validation"));
        }

        SECTION("Fails if listed include directory is missing")
        {
            Certificate cert;
            checker.validate("tests/data/build_description/fail/missing_dir", cert);
            CHECK(has_fail(cert, "Build Description Semantic Validation"));
        }

        SECTION("Passes if all listed entries exist")
        {
            Certificate cert;
            checker.validate("tests/data/build_description/pass/valid", cert);
            CHECK(has_pass(cert, "Build Description Semantic Validation"));
        }

        SECTION("Warns about unlisted source file on disk")
        {
            std::filesystem::path path = "tests/data/build_description/warn/unlisted_file";
            Certificate cert;
            checker.validate(path, cert);
            CHECK(has_warn(cert, "Build Description Semantic Validation"));
        }

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
            Certificate cert;
            checker.validate(path, cert);
            CHECK(has_warn(cert, "Build Description Semantic Validation"));
            fs::remove_all(path);
        }

        SECTION("Accepts C++26 language")
        {
            auto path = fs::temp_directory_path() / "bd_cpp26_test";
            fs::remove_all(path);
            fs::create_directories(path / "sources");
            {
                std::ofstream ofs(path / "sources/buildDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><fmiBuildDescription fmiVersion=\"3.0\"><BuildConfiguration modelIdentifier=\"test\" language=\"C++26\"/></fmiBuildDescription>";
            }
            Certificate cert;
            checker.validate(path, cert);
            CHECK(has_pass(cert, "Build Description Semantic Validation"));
            fs::remove_all(path);
        }
    }

    SECTION("New source extensions")
    {
        BuildDescriptionChecker checker("3.0");
        auto path = fs::temp_directory_path() / "bd_ext_test";
        fs::remove_all(path);
        fs::create_directories(path / "sources");

        SECTION("Fails if version mismatch")
        {
            auto path = fs::temp_directory_path() / "fmi_version_mismatch";
            fs::remove_all(path);
            fs::create_directories(path / "sources");
            {
                std::ofstream ofs(path / "sources/buildDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><fmiBuildDescription fmiVersion=\"2.0\"/>";
            }
            Certificate cert;
            checker.validate(path, cert);
            CHECK(has_fail(cert, "Build Description Semantic Validation"));
            fs::remove_all(path);
        }

        SECTION("Fails if modelIdentifier mismatch")
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
            Certificate cert;
            checker.validate(path, cert);
            CHECK(has_fail(cert, "Build Description Semantic Validation"));
            fs::remove_all(path);
        }

        SECTION("Passes if modelIdentifier matches one of many")
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
            Certificate cert;
            checker.validate(path, cert);
            CHECK(has_pass(cert, "Build Description Semantic Validation"));
            fs::remove_all(path);
        }

        SECTION("Fails if path contains ..")
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
            Certificate cert;
            checker.validate(path, cert);
            CHECK(has_fail(cert, "Build Description Semantic Validation"));
            fs::remove_all(path);
        }
    }

    SECTION("Attributes validation")
    {
        BuildDescriptionChecker checker("3.0");
        auto path = fs::temp_directory_path() / "bd_attr_test";
        fs::remove_all(path);
        fs::create_directories(path / "sources");

        SECTION("Warns about unknown language")
        {
            {
                std::ofstream ofs(path / "sources/buildDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><fmiBuildDescription fmiVersion=\"3.0\"><BuildConfiguration modelIdentifier=\"test\" language=\"rust\"/></fmiBuildDescription>";
            }
            Certificate cert;
            checker.validate(path, cert);
            CHECK(has_warn(cert, "Build Description Semantic Validation"));
        }

        SECTION("Warns about unknown compiler")
        {
            {
                std::ofstream ofs(path / "sources/buildDescription.xml");
                ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><fmiBuildDescription fmiVersion=\"3.0\"><BuildConfiguration modelIdentifier=\"test\" compiler=\"mycc\"/></fmiBuildDescription>";
            }
            Certificate cert;
            checker.validate(path, cert);
            CHECK(has_warn(cert, "Build Description Semantic Validation"));
        }
        fs::remove_all(path);
    }
}

TEST_CASE("FMI 2.0 legacy source files validation", "[fmi2][sources]")
{
    Fmi2ModelDescriptionChecker checker;

    SECTION("Fails if listed source file in MD is missing")
    {
        Certificate cert;
        checker.validate("tests/data/fmi2/fail/missing_source", cert);
        CHECK(has_fail(cert, "Source Files Semantic Validation (FMI2)"));
    }
}
