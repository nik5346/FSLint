#include "certificate.h"
#include "model_checker.h"
#include "test_helpers.h"
#include "zipper.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <map>

static void create_test_fmu(const std::filesystem::path& path, const std::string& id,
                            const std::map<std::string, std::filesystem::path>& resources = {})
{
    std::filesystem::create_directories(path.parent_path());
    std::filesystem::path tmp_xml = path.string() + ".xml";
    std::ofstream md(tmp_xml);
    md << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
       << "<fmiModelDescription fmiVersion=\"2.0\" modelName=\"" << id << "\" guid=\"{1}\">\n"
       << "  <ModelExchange modelIdentifier=\"" << id << "\"/>\n"
       << "</fmiModelDescription>";
    md.close();

    Zipper zip;
    zip.create(path);
    zip.addFileFromDisk("modelDescription.xml", tmp_xml);
    for (const auto& [name, src] : resources)
    {
        zip.addFileFromDisk("resources/" + name, src);
    }
    zip.close();
    std::filesystem::remove(tmp_xml);
}

TEST_CASE("Recursive Model Validation", "[recursive]")
{
    ModelChecker checker;

    SECTION("Nested FMU structure")
    {
        std::filesystem::path root_path = "tests/data/nested/root";
        std::filesystem::path resources_path = root_path / "resources";

        // Build them in reverse order
        std::filesystem::path deep_fmu = resources_path / "deep_inner.fmu";
        create_test_fmu(deep_fmu, "deep_inner");

        std::filesystem::path even_fmu = resources_path / "even_inner.fmu";
        create_test_fmu(even_fmu, "even_inner", {{"deep_inner.fmu", deep_fmu}});

        std::filesystem::path inner_fmu = resources_path / "inner.fmu";
        create_test_fmu(inner_fmu, "inner", {{"even_inner.fmu", even_fmu}});

        // Remove intermediate ones from the disk to ensure we are testing recursion inside the FMUs
        std::filesystem::remove(deep_fmu);
        std::filesystem::remove(even_fmu);

        Certificate result_cert = checker.validateCore(root_path);

        const auto& nested = result_cert.getNestedModels();
        REQUIRE(nested.size() == 1);
        CHECK(nested[0].name == "inner.fmu");

        REQUIRE(nested[0].nested_models.size() == 1);
        CHECK(nested[0].nested_models[0].name == "even_inner.fmu");

        REQUIRE(nested[0].nested_models[0].nested_models.size() == 1);
        CHECK(nested[0].nested_models[0].nested_models[0].name == "deep_inner.fmu");

        // Cleanup
        std::filesystem::remove(inner_fmu);
    }

    SECTION("SSP structure")
    {
        std::filesystem::path ssp_path = "tests/data/nested/root_ssp";
        std::filesystem::path resources_path = ssp_path / "resources";

        std::filesystem::path inner_fmu = resources_path / "inner.fmu";
        create_test_fmu(inner_fmu, "inner");

        Certificate result_cert = checker.validateCore(ssp_path);

        const auto& nested = result_cert.getNestedModels();
        REQUIRE(nested.size() == 1);
        CHECK(nested[0].name == "inner.fmu");

        // Cleanup
        std::filesystem::remove(inner_fmu);
    }
}
