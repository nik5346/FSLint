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

TEST_CASE("Certificate Verification", "[certificate][verification]")
{
    // Create a temporary directory for testing
    const std::filesystem::path base_test_dir = "tests/data/fmi2/pass";
    const std::filesystem::path temp_test_dir = std::filesystem::temp_directory_path() / "fslint_verify_test";

    if (std::filesystem::exists(temp_test_dir))
        std::filesystem::remove_all(temp_test_dir);

    std::filesystem::create_directories(temp_test_dir);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(base_test_dir))
    {
        const auto rel_path = std::filesystem::relative(entry.path(), base_test_dir);
        if (entry.is_directory())
            std::filesystem::create_directories(temp_test_dir / rel_path);
        else
            std::filesystem::copy_file(entry.path(), temp_test_dir / rel_path);
    }

    ModelChecker checker;

    SECTION("Successful verification")
    {
        REQUIRE(checker.addCertificate(temp_test_dir));
        CHECK(checker.verifyCertificate(temp_test_dir));
    }

    SECTION("Failed verification after modification")
    {
        REQUIRE(checker.addCertificate(temp_test_dir));
        CHECK(checker.verifyCertificate(temp_test_dir));

        // Modify modelDescription.xml
        std::ofstream ofs(temp_test_dir / "modelDescription.xml", std::ios::app);
        ofs << "\n<!-- modified -->\n";
        ofs.close();

        CHECK_FALSE(checker.verifyCertificate(temp_test_dir));
    }

    SECTION("Failed verification after adding a file")
    {
        REQUIRE(checker.addCertificate(temp_test_dir));
        CHECK(checker.verifyCertificate(temp_test_dir));

        // Add a new file
        std::ofstream ofs(temp_test_dir / "new_file.txt");
        ofs << "new file content";
        ofs.close();

        CHECK_FALSE(checker.verifyCertificate(temp_test_dir));
    }

    SECTION("Failed verification after removing a file")
    {
        REQUIRE(checker.addCertificate(temp_test_dir));
        CHECK(checker.verifyCertificate(temp_test_dir));

        // Remove a file (not the certificate)
        std::filesystem::remove(temp_test_dir / "modelDescription.xml");

        CHECK_FALSE(checker.verifyCertificate(temp_test_dir));
    }

    // Cleanup
    std::filesystem::remove_all(temp_test_dir);
}
