#include "model_checker.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("ModelChecker with directories", "[directory]")
{
    std::filesystem::path test_dir = "tests/data/directory_model";
    std::filesystem::create_directories(test_dir);

    {
        std::ofstream md(test_dir / "modelDescription.xml");
        md << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << "<fmiModelDescription fmiVersion=\"2.0\" modelName=\"test\" guid=\"{12345678-1234-1234-1234-123456789012}\">\n"
           << "  <ModelVariables>\n"
           << "    <ScalarVariable name=\"v\" valueReference=\"1\" causality=\"local\" variability=\"continuous\">\n"
           << "      <Real/>\n"
           << "    </ScalarVariable>\n"
           << "  </ModelVariables>\n"
           << "  <ModelStructure/>\n"
           << "</fmiModelDescription>";
    }

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

    std::filesystem::path ssp_dir = "tests/data/directory_ssp";
    std::filesystem::create_directories(ssp_dir);

    {
        std::ofstream ssd(ssp_dir / "SystemStructure.ssd");
        ssd << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            << "<SystemStructureDescription xmlns=\"http://ssp-standard.org/SSP1/SystemStructureDescription\" version=\"1.0\" name=\"test\">\n"
            << "</SystemStructureDescription>";
    }

    SECTION("SSP Validation")
    {
        checker.validate(ssp_dir);
    }

    // Cleanup
    std::filesystem::remove_all(ssp_dir);
}
