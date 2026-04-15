#include "model_checker.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Certificate Management on Directories", "[certificate][directory]")
{
    // Use reference FMU extracted for directory testing
    const std::filesystem::path fmu_path = "tests/reference_fmus/2.0/BouncingBall.fmu";
    const std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "fslint_dir_cert_test";
    ModelChecker checker;

    if (std::filesystem::exists(test_dir))
        std::filesystem::remove_all(test_dir);
    REQUIRE(checker.extract(fmu_path, test_dir));

    // Ensure clean state before starting
    (void)checker.removeCertificate(test_dir);

    SECTION("Validation")
    {
        // Should not throw and should complete
        (void)checker.validate(test_dir);
    }

    SECTION("Certificate Operations")
    {
        // Add
        bool added = checker.addCertificate(test_dir);
        CHECK(added);
        CHECK(std::filesystem::exists(test_dir / "extra/org.fslint/cert.txt"));

        // Display
        bool displayed = checker.displayCertificate(test_dir);
        CHECK(displayed);

        // Update
        bool updated = checker.updateCertificate(test_dir);
        CHECK(updated);
        CHECK(std::filesystem::exists(test_dir / "extra/org.fslint/cert.txt"));

        // Remove
        bool removed = checker.removeCertificate(test_dir);
        CHECK(removed);
        CHECK_FALSE(std::filesystem::exists(test_dir / "extra/org.fslint/cert.txt"));
    }

    // Final cleanup to ensure no side effects on other tests
    (void)checker.removeCertificate(test_dir);
}

TEST_CASE("Certificate Verification", "[certificate][verification]")
{
    // Create a temporary directory for testing
    const std::filesystem::path fmu_path = "tests/reference_fmus/2.0/BouncingBall.fmu";
    const std::filesystem::path temp_test_dir = std::filesystem::temp_directory_path() / "fslint_verify_test";

    if (std::filesystem::exists(temp_test_dir))
        std::filesystem::remove_all(temp_test_dir);

    ModelChecker checker;
    REQUIRE(checker.extract(fmu_path, temp_test_dir));

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
