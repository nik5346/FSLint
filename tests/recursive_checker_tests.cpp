#include "certificate.h"
#include "model_checker.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

TEST_CASE("Recursive Model Validation", "[recursive]")
{
    ModelChecker checker;

    SECTION("Nested FMU structure")
    {
        std::filesystem::path root_path = "tests/data/nested/root";

        Certificate result_cert = checker.validate(root_path, true);

        const auto& nested = result_cert.getNestedModels();
        REQUIRE(nested.size() == 1);
        CHECK(nested[0].name == "inner.fmu/");
        CHECK(nested[0].logical_path == "inner.fmu");

        REQUIRE(nested[0].nested_models.size() == 1);
        CHECK(nested[0].nested_models[0].name == "even_inner.fmu/");
        CHECK(nested[0].nested_models[0].logical_path == "inner.fmu/even_inner.fmu");

        REQUIRE(nested[0].nested_models[0].nested_models.size() == 1);
        CHECK(nested[0].nested_models[0].nested_models[0].name == "deep_inner.fmu/");
        CHECK(nested[0].nested_models[0].nested_models[0].logical_path == "inner.fmu/even_inner.fmu/deep_inner.fmu");
    }

    SECTION("FMI 1.0 Nested FMU structure")
    {
        std::filesystem::path root_path = "tests/data/fmi1/pass/nested";

        Certificate result_cert = checker.validate(root_path, true);

        const auto& nested = result_cert.getNestedModels();
        REQUIRE(nested.size() == 1);
        CHECK(nested[0].name == "inner.fmu/");
    }

    SECTION("SSP structure")
    {
        std::filesystem::path ssp_path = "tests/data/nested/root_ssp";

        Certificate result_cert = checker.validate(ssp_path, true);

        const auto& nested = result_cert.getNestedModels();
        REQUIRE(nested.size() == 1);
        CHECK(nested[0].name == "inner.fmu/");
    }
}
