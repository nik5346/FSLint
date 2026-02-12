#include "certificate.h"
#include "model_checker.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Recursive Model Validation", "[recursive]")
{
    SECTION("Nested FMU structure")
    {
        ModelChecker checker;
        std::string path = "tests/data/recursive/basic";
        Certificate cert = checker.validateCore(path);

        INFO("Checking path: " << path);
        CHECK_FALSE(cert.getNestedModels().empty());

        bool found_nested = false;
        bool found_ssp = false;

        for (const auto& nested : cert.getNestedModels())
        {
            if (nested.name == "nested.fmu")
                found_nested = true;
            if (nested.name == "system.ssp")
                found_ssp = true;
        }

        CHECK(found_nested);
        CHECK(found_ssp);
    }
}
