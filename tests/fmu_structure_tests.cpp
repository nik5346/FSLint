#include "certificate.h"
#include "directory_checker.h"
#include "build_description_checker.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

void create_fmu_dir(const fs::path& root, const std::string& fmi_version, const std::string& model_id) {
    fs::create_directories(root);
    std::ofstream md(root / "modelDescription.xml");
    md << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    md << "<fmiModelDescription fmiVersion=\"" << fmi_version << "\" modelName=\"test\" guid=\"{123}\">\n";
    md << "  <CoSimulation modelIdentifier=\"" << model_id << "\"/>\n";
    md << "  <ModelVariables>\n";
    md << "    <ScalarVariable name=\"v\" valueReference=\"1\" causality=\"output\"><Real/></ScalarVariable>\n";
    md << "  </ModelVariables>\n";
    md << "  <ModelStructure><Outputs><Unknown index=\"1\"/></Outputs></ModelStructure>\n";
    md << "</fmiModelDescription>\n";
}

TEST_CASE("DirectoryChecker distribution validation", "[directory]") {
    DirectoryChecker checker;
    fs::path test_dir = fs::current_path() / "test_fmu_structure";

    SECTION("Fails if neither binaries nor sources") {
        create_fmu_dir(test_dir, "2.0", "test");
        Certificate cert;
        checker.validate(test_dir, cert);

        bool has_fail = false;
        for (const auto& res : cert.getResults()) {
            if (res.status == TestStatus::FAIL) {
                for (const auto& msg : res.messages) {
                    if (msg.find("must contain either a precompiled binary for at least one platform or source code") != std::string::npos) {
                        has_fail = true;
                    }
                }
            }
        }
        CHECK(has_fail);
        fs::remove_all(test_dir);
    }

    SECTION("Passes with binaries") {
        create_fmu_dir(test_dir, "2.0", "test");
        fs::create_directories(test_dir / "binaries" / "win64");
        std::ofstream bin(test_dir / "binaries" / "win64" / "test.dll");
        bin << "dummy";

        Certificate cert;
        checker.validate(test_dir, cert);

        bool has_fail = false;
        for (const auto& res : cert.getResults()) {
            if (res.status == TestStatus::FAIL) has_fail = true;
        }
        CHECK_FALSE(has_fail);
        fs::remove_all(test_dir);
    }

    SECTION("Passes with sources in MD") {
        fs::create_directories(test_dir);
        std::ofstream md(test_dir / "modelDescription.xml");
        md << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << "<fmiModelDescription fmiVersion=\"2.0\" modelName=\"test\" guid=\"{123}\">\n"
           << "  <CoSimulation modelIdentifier=\"test\">\n"
           << "    <SourceFiles><File name=\"test.c\"/></SourceFiles>\n"
           << "  </CoSimulation>\n"
           << "  <ModelVariables>\n"
           << "    <ScalarVariable name=\"v\" valueReference=\"1\" causality=\"output\"><Real/></ScalarVariable>\n"
           << "  </ModelVariables>\n"
           << "  <ModelStructure><Outputs><Unknown index=\"1\"/></Outputs></ModelStructure>\n"
           << "</fmiModelDescription>\n";
        md.close();

        Certificate cert;
        checker.validate(test_dir, cert);

        bool has_fail = false;
        for (const auto& res : cert.getResults()) {
            if (res.status == TestStatus::FAIL) has_fail = true;
        }
        CHECK_FALSE(has_fail);
        fs::remove_all(test_dir);
    }

    SECTION("Passes with buildDescription.xml") {
        create_fmu_dir(test_dir, "2.0", "test");
        fs::create_directories(test_dir / "sources");
        std::ofstream bd(test_dir / "sources" / "buildDescription.xml");
        bd << "dummy";

        Certificate cert;
        checker.validate(test_dir, cert);

        bool has_fail = false;
        for (const auto& res : cert.getResults()) {
            if (res.status == TestStatus::FAIL) has_fail = true;
        }
        CHECK_FALSE(has_fail);
        fs::remove_all(test_dir);
    }
}

TEST_CASE("BuildDescriptionChecker validation", "[build_description]") {
    BuildDescriptionChecker checker;
    fs::path test_dir = fs::current_path() / "test_fmu_build_desc";
    fs::create_directories(test_dir / "sources");

    SECTION("Fails if listed source file is missing") {
        std::ofstream bd(test_dir / "sources" / "buildDescription.xml");
        bd << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << "<fmiBuildDescription fmiVersion=\"3.0\">\n"
           << "  <SourceFileSet>\n"
           << "    <SourceFile name=\"missing.c\"/>\n"
           << "  </SourceFileSet>\n"
           << "</fmiBuildDescription>\n";
        bd.close();

        Certificate cert;
        checker.validate(test_dir, cert);

        bool has_fail = false;
        for (const auto& res : cert.getResults()) {
            if (res.status == TestStatus::FAIL) {
                for (const auto& msg : res.messages) {
                    if (msg.find("Source file 'missing.c' listed in 'buildDescription.xml'") != std::string::npos) {
                        has_fail = true;
                    }
                }
            }
        }
        CHECK(has_fail);
    }

    SECTION("Fails if listed include directory is missing") {
        std::ofstream bd(test_dir / "sources" / "buildDescription.xml");
        bd << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << "<fmiBuildDescription fmiVersion=\"3.0\">\n"
           << "  <SourceFileSet>\n"
           << "    <IncludeDirectory name=\"missing_inc\"/>\n"
           << "  </SourceFileSet>\n"
           << "</fmiBuildDescription>\n";
        bd.close();

        Certificate cert;
        checker.validate(test_dir, cert);

        bool has_fail = false;
        for (const auto& res : cert.getResults()) {
            if (res.status == TestStatus::FAIL) {
                for (const auto& msg : res.messages) {
                    if (msg.find("Include directory 'missing_inc' listed in 'buildDescription.xml'") != std::string::npos) {
                        has_fail = true;
                    }
                }
            }
        }
        CHECK(has_fail);
    }

    SECTION("Passes if all listed entries exist") {
        std::ofstream bd(test_dir / "sources" / "buildDescription.xml");
        bd << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << "<fmiBuildDescription fmiVersion=\"3.0\">\n"
           << "  <SourceFileSet>\n"
           << "    <SourceFile name=\"exists.c\"/>\n"
           << "    <IncludeDirectory name=\"exists_inc\"/>\n"
           << "  </SourceFileSet>\n"
           << "</fmiBuildDescription>\n";
        bd.close();

        std::ofstream src(test_dir / "sources" / "exists.c");
        src << "int x;";
        src.close();
        fs::create_directories(test_dir / "sources" / "exists_inc");

        Certificate cert;
        checker.validate(test_dir, cert);

        bool has_fail = false;
        for (const auto& res : cert.getResults()) {
            if (res.status == TestStatus::FAIL) has_fail = true;
        }
        CHECK_FALSE(has_fail);
    }

    fs::remove_all(test_dir);
}

#include "fmi2_model_description_checker.h"

TEST_CASE("Fmi2ModelDescriptionChecker source files validation", "[fmi2_sources]") {
    Fmi2ModelDescriptionChecker checker;
    fs::path test_dir = fs::current_path() / "test_fmi2_sources";
    fs::create_directories(test_dir / "sources");

    SECTION("Fails if listed source file in MD is missing") {
        std::ofstream md(test_dir / "modelDescription.xml");
        md << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << "<fmiModelDescription fmiVersion=\"2.0\" modelName=\"test\" guid=\"{123}\">\n"
           << "  <CoSimulation modelIdentifier=\"test\">\n"
           << "    <SourceFiles><File name=\"missing.c\"/></SourceFiles>\n"
           << "  </CoSimulation>\n"
           << "  <ModelVariables><ScalarVariable name=\"v\" valueReference=\"1\"><Real/></ScalarVariable></ModelVariables>\n"
           << "  <ModelStructure/>\n"
           << "</fmiModelDescription>\n";
        md.close();

        Certificate cert;
        checker.validate(test_dir, cert);

        bool has_fail = false;
        for (const auto& res : cert.getResults()) {
            if (res.status == TestStatus::FAIL && res.test_name == "Source Files Existence (FMI2)") {
                for (const auto& msg : res.messages) {
                    if (msg.find("Source file 'missing.c' listed in 'modelDescription.xml'") != std::string::npos) {
                        has_fail = true;
                    }
                }
            }
        }
        CHECK(has_fail);
    }

    SECTION("Warns if only SourceFiles is present") {
        std::ofstream md(test_dir / "modelDescription.xml");
        md << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << "<fmiModelDescription fmiVersion=\"2.0\" modelName=\"test\" guid=\"{123}\">\n"
           << "  <CoSimulation modelIdentifier=\"test\">\n"
           << "    <SourceFiles><File name=\"exists.c\"/></SourceFiles>\n"
           << "  </CoSimulation>\n"
           << "  <ModelVariables><ScalarVariable name=\"v\" valueReference=\"1\"><Real/></ScalarVariable></ModelVariables>\n"
           << "  <ModelStructure/>\n"
           << "</fmiModelDescription>\n";
        md.close();
        std::ofstream src(test_dir / "sources" / "exists.c"); src << "int x;"; src.close();

        Certificate cert;
        checker.validate(test_dir, cert);

        bool has_warn = false;
        for (const auto& res : cert.getResults()) {
            if (res.status == TestStatus::WARNING && res.test_name == "Source Files Existence (FMI2)") {
                for (const auto& msg : res.messages) {
                    if (msg.find("only contains <SourceFiles> in modelDescription.xml") != std::string::npos) {
                        has_warn = true;
                    }
                }
            }
        }
        CHECK(has_warn);
    }

    SECTION("Warns if only buildDescription.xml is present") {
        std::ofstream md(test_dir / "modelDescription.xml");
        md << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << "<fmiModelDescription fmiVersion=\"2.0\" modelName=\"test\" guid=\"{123}\">\n"
           << "  <CoSimulation modelIdentifier=\"test\"/>\n"
           << "  <ModelVariables><ScalarVariable name=\"v\" valueReference=\"1\"><Real/></ScalarVariable></ModelVariables>\n"
           << "  <ModelStructure/>\n"
           << "</fmiModelDescription>\n";
        md.close();
        std::ofstream bd(test_dir / "sources" / "buildDescription.xml"); bd << "dummy"; bd.close();

        Certificate cert;
        checker.validate(test_dir, cert);

        bool has_warn = false;
        for (const auto& res : cert.getResults()) {
            if (res.status == TestStatus::WARNING && res.test_name == "Source Files Existence (FMI2)") {
                for (const auto& msg : res.messages) {
                    if (msg.find("only contains buildDescription.xml") != std::string::npos) {
                        has_warn = true;
                    }
                }
            }
        }
        CHECK(has_warn);
    }

    fs::remove_all(test_dir);
}
