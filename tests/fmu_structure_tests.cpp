#include "build_description_checker.h"
#include "certificate.h"
#include "checker_factory.h"
#include "directory_checker.h"
#include "fmi2_model_description_checker.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

bool has_test_with_status(const Certificate& cert, const std::string& test_name, TestStatus status)
{
    const auto& results = cert.getResults();
    std::cout << "DEBUG: Searching for '" << test_name << "' with status " << (int)status << std::endl;
    for (const auto& r : results)
    {
        std::cout << "DEBUG: Found '" << r.test_name << "' with status " << (int)r.status << std::endl;
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
    DirectoryChecker checker;

    SECTION("Fails if neither binaries nor sources")
    {
        Certificate cert;
        checker.validate("tests/data/directory/fail/no_impl", cert);
        CHECK(has_fail(cert, "FMU Structure"));
    }

    SECTION("Passes with binaries")
    {
        Certificate cert;
        checker.validate("tests/data/directory/pass/binaries", cert);
        CHECK(has_pass(cert, "FMU Structure"));
    }

    SECTION("Passes with sources")
    {
        Certificate cert;
        checker.validate("tests/data/directory/pass/sources", cert);
        CHECK(has_pass(cert, "FMU Structure"));
    }

    SECTION("Warns about unknown entry in root")
    {
        Certificate cert;
        checker.validate("tests/data/directory/warn/unknown_entry", cert);
        CHECK(has_warn(cert, "FMU Structure"));
    }
}

TEST_CASE("BuildDescriptionChecker validation", "[build_description]")
{
    BuildDescriptionChecker checker;

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
        auto info = CheckerFactory::detectModel(path);
        BuildDescriptionChecker checker;
        Certificate cert;
        checker.validate(path, cert);
        CHECK(has_warn(cert, "Build Description Semantic Validation"));
    }
}

TEST_CASE("FMI 2.0 source files and distribution validation", "[fmi2][sources]")
{
    Fmi2ModelDescriptionChecker checker;

    SECTION("Fails if listed source file in MD is missing")
    {
        Certificate cert;
        checker.validate("tests/data/fmi2/fail/missing_source", cert);
        CHECK(has_fail(cert, "FMU Distribution"));
    }

    SECTION("Warns if only SourceFiles is present")
    {
        Certificate cert;
        checker.validate("tests/data/fmi2/warn/dist_sources_only", cert);
        CHECK(has_warn(cert, "FMU Distribution"));
    }

    SECTION("Warns if only buildDescription.xml is present")
    {
        Certificate cert;
        checker.validate("tests/data/fmi2/warn/dist_build_desc_only", cert);
        CHECK(has_warn(cert, "FMU Distribution"));
    }

    SECTION("Passes if both are present")
    {
        Certificate cert;
        checker.validate("tests/data/fmi2/pass/dist_both", cert);
        CHECK(has_pass(cert, "FMU Distribution"));
    }

    SECTION("Warns about unlisted legacy source file on disk")
    {
        std::filesystem::path path = "tests/data/fmi2/warn/legacy_unlisted_file";
        auto info = CheckerFactory::detectModel(path);
        Fmi2ModelDescriptionChecker checker;
        Certificate cert;
        checker.validate(path, cert);
        CHECK(has_warn(cert, "FMU Distribution"));
    }
}
