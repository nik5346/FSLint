#include "model_checker.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Certificate Management on Directories", "[certificate][directory]")
{
    // Use existing test data directly
    std::filesystem::path test_dir = "tests/data/fmi2/pass";
    ModelChecker checker;

    // Ensure clean state before starting
    checker.removeCertificate(test_dir);

    SECTION("Validation")
    {
        // Should not throw and should complete
        checker.validate(test_dir);
    }

    SECTION("Certificate Operations")
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

    // Final cleanup to ensure no side effects on other tests
    checker.removeCertificate(test_dir);
}
