#include <catch2/catch_test_macros.hpp>
#include "fmi2_model_description_checker.h"
#include "fmi3_model_description_checker.h"
#include "certificate.h"
#include <filesystem>
#include <algorithm>

bool has_fail(const Certificate& cert) {
    const auto& results = cert.getResults();
    return std::any_of(results.begin(), results.end(), [](const TestResult& r) {
        return r.status == TestStatus::FAIL;
    });
}

TEST_CASE("FMI 2.0 Model Description Validation", "[fmi2]") {
    Fmi2ModelDescriptionChecker checker;
    Certificate cert;

    SECTION("Valid FMI 2.0") {
        std::filesystem::path path = "tests/data/fmi2/valid";
        checker.validate(path, cert);

        REQUIRE_FALSE(has_fail(cert));
    }

    SECTION("Invalid GUID FMI 2.0") {
        std::filesystem::path path = "tests/data/fmi2/invalid_guid";
        checker.validate(path, cert);

        REQUIRE(has_fail(cert));

        bool found_guid_error = false;
        for (const auto& res : cert.getResults()) {
            if (res.test_name == "GUID Format" && res.status == TestStatus::FAIL) {
                found_guid_error = true;
                break;
            }
        }
        CHECK(found_guid_error);
    }

    SECTION("Duplicate Variable Names FMI 2.0") {
        std::filesystem::path path = "tests/data/fmi2/duplicate_variables";
        checker.validate(path, cert);

        REQUIRE(has_fail(cert));

        bool found_duplicate_error = false;
        for (const auto& res : cert.getResults()) {
            if (res.test_name == "Unique Variable Names" && res.status == TestStatus::FAIL) {
                found_duplicate_error = true;
                break;
            }
        }
        CHECK(found_duplicate_error);
    }
}

TEST_CASE("FMI 3.0 Model Description Validation", "[fmi3]") {
    Fmi3ModelDescriptionChecker checker;
    Certificate cert;

    SECTION("Valid FMI 3.0") {
        std::filesystem::path path = "tests/data/fmi3/valid";
        checker.validate(path, cert);

        REQUIRE_FALSE(has_fail(cert));
    }

    SECTION("Invalid Instantiation Token FMI 3.0") {
        std::filesystem::path path = "tests/data/fmi3/invalid_token";
        checker.validate(path, cert);

        REQUIRE(has_fail(cert));

        bool found_token_error = false;
        for (const auto& res : cert.getResults()) {
            if (res.test_name == "Instantiation Token Format" && res.status == TestStatus::FAIL) {
                found_token_error = true;
                break;
            }
        }
        CHECK(found_token_error);
    }
}
