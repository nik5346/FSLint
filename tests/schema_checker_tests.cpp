#include "certificate.h"
#include "fmi1_schema_checker.h"
#include "fmi2_schema_checker.h"
#include "fmi3_schema_checker.h"
#include "ssp1_schema_checker.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>

class SchemaPathTestChecker : public SchemaCheckerBase
{
  public:
    using SchemaCheckerBase::findSchemaPath;
    std::vector<XmlFileRule> getXmlRules(const std::filesystem::path& /*path*/) const override
    {
        return {};
    }
    std::string getStandardName() const override
    {
        return _name;
    }
    std::string getStandardVersion() const override
    {
        return _version;
    }
    void setStandard(const std::string& name, const std::string& version)
    {
        _name = name;
        _version = version;
    }

  private:
    std::string _name;
    std::string _version;
};

TEST_CASE("Schema Path Discovery", "[schema][path]")
{
    SchemaPathTestChecker checker;

    SECTION("FMI 1.0 ME")
    {
        checker.setStandard("fmi", "1.0/ME");
        auto path = checker.findSchemaPath("fmiModelDescription.xsd");
        CHECK(!path.empty());
        CHECK(std::filesystem::exists(path));
    }

    SECTION("FMI 1.0 CS")
    {
        checker.setStandard("fmi", "1.0/CS");
        auto path = checker.findSchemaPath("fmiModelDescription.xsd");
        CHECK(!path.empty());
        CHECK(std::filesystem::exists(path));
    }

    SECTION("FMI 2.0")
    {
        checker.setStandard("fmi", "2.0");
        auto path = checker.findSchemaPath("fmi2ModelDescription.xsd");
        CHECK(!path.empty());
        CHECK(std::filesystem::exists(path));
    }

    SECTION("FMI 2.0.1 Normalization")
    {
        checker.setStandard("fmi", "2.0.1");
        auto path = checker.findSchemaPath("fmi2ModelDescription.xsd");
        CHECK(!path.empty());
        CHECK(std::filesystem::exists(path));
        CHECK(path.string().find("2.0") != std::string::npos);
    }

    SECTION("FMI 3.0")
    {
        checker.setStandard("fmi", "3.0");
        auto path = checker.findSchemaPath("fmi3ModelDescription.xsd");
        CHECK(!path.empty());
        CHECK(std::filesystem::exists(path));
    }

    SECTION("SSP 1.0")
    {
        checker.setStandard("ssp", "1.0");
        auto path = checker.findSchemaPath("SystemStructureDescription.xsd");
        CHECK(!path.empty());
        CHECK(std::filesystem::exists(path));
    }
}

TEST_CASE("FMI 1.0 Encoding Validation", "[fmi1][encoding]")
{
    Fmi1MeSchemaChecker checker;

    SECTION("ISO-8859-1 Warning")
    {
        Certificate cert;
        checker.validate("tests/data/fmi1/warn/encoding_iso", cert);
        CHECK(has_warning_with_text(cert, "It is recommended to use UTF-8"));
        CHECK_FALSE(has_fail(cert));
    }
}

TEST_CASE("FMI 2.0 Encoding Validation", "[fmi2][encoding]")
{
    Fmi2SchemaChecker checker;

    SECTION("ISO-8859-1 Failure")
    {
        Certificate cert;
        checker.validate("tests/data/fmi2/fail/encoding_iso", cert);
        CHECK(has_error_with_text(cert, "Encoding must be UTF-8, found: ISO-8859-1"));
        CHECK(has_fail(cert));
    }

    SECTION("Invalid UTF-8 Content Failure")
    {
        Certificate cert;
        checker.validate("tests/data/fmi2/fail/encoding_invalid_content", cert);
        CHECK(has_error_with_text(cert, "File content is not valid UTF-8"));
        CHECK(has_fail(cert));
    }
}
