#include "certificate.h"
#include "checker_factory.h"
#include "fmi1_binary_checker.h"
#include "fmi1_directory_checker.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("FMI 1.0 Factory Detection", "[fmi1][factory]")
{
    SECTION("FMI 1.0 ME")
    {
        auto info = CheckerFactory::detectModel("tests/data/fmi1/pass/me");
        CHECK(info.standard == ModelStandard::FMI1_ME);
        CHECK(info.version == "1.0");

        auto checkers = CheckerFactory::createCheckers(info);
        bool found_dir = false;
        bool found_bin = false;
        for (const auto& c : checkers)
        {
            if (dynamic_cast<Fmi1DirectoryChecker*>(c.get()))
                found_dir = true;
            if (dynamic_cast<Fmi1BinaryChecker*>(c.get()))
                found_bin = true;
        }
        CHECK(found_dir);
        CHECK(found_bin);
    }

    SECTION("FMI 1.0 CS")
    {
        auto info = CheckerFactory::detectModel("tests/data/fmi1/pass/cs");
        CHECK(info.standard == ModelStandard::FMI1_CS);
        CHECK(info.version == "1.0");
    }
}

TEST_CASE("FMI 1.0 Directory Checker", "[fmi1][directory]")
{
    Fmi1DirectoryChecker checker;
    Certificate cert;

    SECTION("Valid ME Structure")
    {
        checker.validate("tests/data/fmi1/pass/me", cert);
        CHECK_FALSE(has_fail(cert));
    }
}
