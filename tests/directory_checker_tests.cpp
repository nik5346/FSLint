#include "model_checker.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("ModelChecker with directories", "[directory]")
{
    // Use existing test data as a source
    std::filesystem::path source_fmu = "tests/data/fmi2/pass";
    std::filesystem::path test_dir = "tests/data/directory_model_test";

    if (std::filesystem::exists(test_dir)) {
        std::filesystem::remove_all(test_dir);
    }
    std::filesystem::create_directories(test_dir);
    std::filesystem::copy(source_fmu, test_dir, std::filesystem::copy_options::recursive);

    ModelChecker checker;

    SECTION("Validation")
    {
        // Should not throw and should complete
        checker.validate(test_dir);
    }

    SECTION("Certificate Management")
    {
        // Add
        bool added = checker.addCertificate(test_dir);
        CHECK(added);
        CHECK(std::filesystem::exists(test_dir / "extra/validation_certificate.txt"));

        // Display
        bool displayed = checker.displayCertificate(test_dir);
        CHECK(displayed);

        // Update
        bool updated = checker.updateCertificate(test_dir);
        CHECK(updated);
        CHECK(std::filesystem::exists(test_dir / "extra/validation_certificate.txt"));

        // Remove
        bool removed = checker.removeCertificate(test_dir);
        CHECK(removed);
        CHECK_FALSE(std::filesystem::exists(test_dir / "extra/validation_certificate.txt"));
    }

    // Cleanup
    std::filesystem::remove_all(test_dir);
}
