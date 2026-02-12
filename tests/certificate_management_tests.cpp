#include "model_checker.h"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Certificate Management on Directories", "[certificate][directory]")
{
    // Create a temporary directory to avoid modifying checked-in test data
    auto now = std::chrono::high_resolution_clock::now();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / ("cert_mgmt_test_" + std::to_string(nanos));
    std::filesystem::create_directories(temp_dir);

    // Populate with a minimal FMI 2.0 structure
    std::ofstream(temp_dir / "modelDescription.xml") << R"(<fmiModelDescription fmiVersion="2.0" modelName="Test" guid="123">
  <CoSimulation modelIdentifier="Test"/>
</fmiModelDescription>)";

    ModelChecker checker;

    SECTION("Validation")
    {
        // Should not throw and should complete
        checker.validate(temp_dir);
    }

    SECTION("Certificate Operations")
    {
        // Add
        bool added = checker.addCertificate(temp_dir);
        CHECK(added);
        CHECK(std::filesystem::exists(temp_dir / "extra" / "validation_certificate.txt"));

        // Display
        bool displayed = checker.displayCertificate(temp_dir);
        CHECK(displayed);

        // Update
        bool updated = checker.updateCertificate(temp_dir);
        CHECK(updated);
        CHECK(std::filesystem::exists(temp_dir / "extra" / "validation_certificate.txt"));

        // Remove
        bool removed = checker.removeCertificate(temp_dir);
        CHECK(removed);
        CHECK_FALSE(std::filesystem::exists(temp_dir / "extra" / "validation_certificate.txt"));
    }

    // Final cleanup
    std::filesystem::remove_all(temp_dir);
}
