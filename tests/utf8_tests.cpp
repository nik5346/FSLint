#include "certificate.h"
#include "file_utils.h"
#include "model_checker.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("UTF-8 Path and Filename Support", "[utf8]")
{
    // Static test data in tests/data/utf8/pass/🚀
    // Contains:
    //  - modelDescription.xml
    //  - sources/
    //  - resources/📁.txt

    fs::path test_data_root = u8"tests/data/utf8/pass/🚀";

    REQUIRE(fs::exists(test_data_root));
    REQUIRE(fs::exists(test_data_root / u8"modelDescription.xml"));
    REQUIRE(fs::exists(test_data_root / "resources" / u8"📁.txt"));

    SECTION("Validate directory model with UTF-8")
    {
        ModelChecker checker;
        Certificate cert = checker.validate(test_data_root, true);

        INFO("Checking results for " << file_utils::pathToUtf8(test_data_root));
        for (const auto& res : cert.getResults())
        {
            if (res.status == TestStatus::FAIL)
                for (const auto& msg : res.messages)
                    UNSCOPED_INFO("FAIL: " << res.test_name << ": " << msg);
        }

        CHECK_FALSE(has_fail(cert));
    }

    SECTION("Validate ZIP model with UTF-8")
    {
        fs::path zip_path = fs::temp_directory_path() / u8"test_🚀.fmu";
        if (fs::exists(zip_path))
            fs::remove(zip_path);

        ModelChecker checker;
        // This will create the ZIP from the static test data
        bool packaged = checker.package(test_data_root, zip_path);
        REQUIRE(packaged);
        REQUIRE(fs::exists(zip_path));

        Certificate cert = checker.validate(zip_path, true);
        CHECK_FALSE(has_fail(cert));

        fs::remove(zip_path);
    }

    SECTION("Validate nested UTF-8 FMU")
    {
        // Create a temporary directory to avoid polluting tests/data
        fs::path temp_dir = fs::temp_directory_path() / u8"utf8_nested_test_🚀";
        if (fs::exists(temp_dir))
            fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);

        // Copy static data to temp dir
        fs::copy(test_data_root, temp_dir, fs::copy_options::recursive);

        std::string nested_name = file_utils::pathToUtf8(u8"nested_🚀.fmu");
        fs::path nested_fmu_path = temp_dir / "resources" / file_utils::utf8ToPath(nested_name);

        ModelChecker checker;
        // Package the same static data as a nested FMU
        bool packaged = checker.package(test_data_root, nested_fmu_path);
        REQUIRE(packaged);

        Certificate cert = checker.validate(temp_dir, true);
        CHECK_FALSE(has_fail(cert));

        // Check if nested model was found
        bool found = false;
        for (const auto& nested : cert.getNestedModels())
        {
            if (nested.name == nested_name)
            {
                found = true;
                break;
            }
        }
        CHECK(found);

        fs::remove_all(temp_dir);
    }
}
