#include "certificate.h"
#include "model_checker.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

TEST_CASE("Recursive Model Validation", "[recursive]")
{
    ModelChecker checker;

    /* Binary FMUs removed - disabling sections
    SECTION("Nested FMU structure")
    {
        // Use the test data created earlier
        std::filesystem::path root_path = "tests/data/nested/root";

        Certificate result_cert = checker.validateCore(root_path);

        const auto& nested = result_cert.getNestedModels();
        REQUIRE(nested.size() == 1);
        CHECK(nested[0].name == "inner.fmu");

        REQUIRE(nested[0].nested_models.size() == 1);
        CHECK(nested[0].nested_models[0].name == "even_inner.fmu");

        REQUIRE(nested[0].nested_models[0].nested_models.size() == 1);
        CHECK(nested[0].nested_models[0].nested_models[0].name == "deep_inner.fmu");
    }

    SECTION("SSP structure")
    {
        std::filesystem::path ssp_path = "tests/data/nested/root_ssp";

        Certificate result_cert = checker.validateCore(ssp_path);

        const auto& nested = result_cert.getNestedModels();
        REQUIRE(nested.size() == 1);
        CHECK(nested[0].name == "inner.fmu");
    }
    */
}
