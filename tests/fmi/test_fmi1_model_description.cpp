#include "certificate.h"
#include "fmi1_model_description_checker.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("FMI 1.0 Model Description Failure Cases", "[fmi1][fail]")
{
    Fmi1ModelDescriptionChecker checker;

    auto validate_fail = [&](const std::string& path, const std::string& expected_error)
    {
        Certificate cert;
        checker.validate("tests/data/fmi1/fail/" + path, cert);
        INFO("Checking path: " << path);
        REQUIRE(has_fail(cert));
        CHECK(has_error_with_text(cert, expected_error));
    };

    SECTION("Metadata")
    {
        validate_fail("metadata/fmi_version_invalid", "is invalid (must be exactly \"1.0\")");
        validate_fail("metadata/guid_missing", "guid attribute is missing");
        validate_fail("metadata/guid_empty", "guid attribute is empty");
        validate_fail("metadata/guid_invalid", "does not match expected GUID format");

        validate_fail("model_name_missing", "modelName attribute is missing");
        validate_fail("model_name_empty", "modelName attribute is empty");

        validate_fail("date_invalid", "is out of range");
        validate_fail("date_future", "is in the future");
        validate_fail("date_format", "does not match ISO 8601 format");
    }

    SECTION("Model Identifier")
    {
        validate_fail("model_identifier_invalid", "cannot start with a digit");
        validate_fail("model_identifier_too_long", "is too long");
    }

    SECTION("Aliases")
    {
        validate_fail("alias_inconsistent_unit", "Variables sharing VR 1 must have the same unit");
        validate_fail("alias_inconsistent_type", "Variables sharing VR 1 must have the same type");
        validate_fail("alias_inconsistent_start", "must have equivalent start values");
        validate_fail("alias_inconsistent_start_negated", "must have equivalent start values");
    }

    SECTION("Implementation")
    {
        validate_fail("implementation/entry_point_missing_file",
                      "references missing file in FMU: 'resources/non_existent.mdl'");
        validate_fail("implementation/file_missing_file",
                      "references missing file in FMU: 'resources/missing_extra.txt'");
        validate_fail("implementation/InvalidUriScheme", "has an unsupported or invalid URI scheme");
        validate_fail("implementation/InvalidHttpUrl", "has an invalid or unsafe HTTP/HTTPS URI");
    }

    SECTION("Model Identifier Filename Match")
    {
        Certificate cert;
        // Explicitly set a mismatched path to test the validation logic
        checker.setOriginalPath("MismatchedName.fmu");
        checker.validate("tests/data/fmi1/pass/TestME", cert);
        REQUIRE(has_fail(cert));
        CHECK(has_error_with_text(cert, "must match the FMU filename 'MismatchedName'"));
    }

    SECTION("Vendor Annotations")
    {
        validate_fail("vendor_annotation_duplicate", "is defined multiple times");
    }

    SECTION("Consistency")
    {
        validate_fail("log_category_duplicate", "is defined multiple times");
        validate_fail("duplicate_name", "is not unique");
        validate_fail("type_duplicate", "is defined multiple times");
        validate_fail("unit_duplicate", "is defined multiple times");
    }

    SECTION("Variable Naming")
    {
        validate_fail("naming_flat_tab", "contains illegal tab character");
        validate_fail("naming_flat_cr", "contains illegal carriage return");
        validate_fail("naming_flat_lf", "contains illegal line feed");
    }

    SECTION("References")
    {
        validate_fail("ref_type_undef", "references undefined type");
        validate_fail("ref_unit_undef", "references undefined unit");
    }

    SECTION("DefaultExperiment")
    {
        validate_fail("exp_start_neg", "startTime");
        validate_fail("exp_stop_less_start", "must be greater than startTime");
        validate_fail("exp_tolerance_zero", "tolerance");
    }

    SECTION("Legal Variability and Combinations")
    {
        validate_fail("variability_continuous_non_real", "cannot have variability \"continuous\"");
        validate_fail("constant_input", "has illegal combination: variability=\"constant\" and causality=\"input\"");
    }

    SECTION("Start Values")
    {
        validate_fail("start_missing_input", "must have a start value");
        validate_fail("start_missing_constant", "must have a start value");
        validate_fail("fixed_no_start", "has 'fixed' attribute but is missing 'start' value");
        validate_fail("fixed_on_input", "has causality=\"input\" and a 'fixed' attribute");
        validate_fail("fixed_on_constant_guess", "has variability=\"constant\" and fixed=\"false\"");
    }
}

TEST_CASE("FMI 1.0 Model Description Warning Cases", "[fmi1][warn]")
{
    Fmi1ModelDescriptionChecker checker;

    auto validate_warning = [&](const std::string& path, const std::string& expected_warning)
    {
        Certificate cert;
        checker.validate("tests/data/fmi1/" + path, cert);
        INFO("Checking path: " << path);
        REQUIRE(has_warning(cert));
        CHECK(has_warning_with_text(cert, expected_warning));
    };

    SECTION("Implementation")
    {
        validate_warning("warn/implementation/ExternalFileMissing",
                         "references an external file that does not exist on this system");
        validate_warning("warn/implementation/UnreachableWebSource",
                         "references a web source that appears to be unreachable");
    }

    SECTION("Metadata")
    {
        validate_warning("warn/generation_date_too_old", "is before the 1.0 standard release (2010)");
        validate_warning("warn/author_missing", "Attribute 'author' is missing");
        validate_warning("warn/author_empty", "Attribute 'author' is empty");
        validate_warning("warn/generation_tool_missing", "Attribute 'generationTool' is missing");
        validate_warning("warn/license_missing", "Attribute 'license' is missing");
        validate_warning("warn/copyright_missing", "Attribute 'copyright' is missing");
        validate_warning("warn/model_version_missing", "Attribute 'version' is missing");
        validate_warning("warn/generation_date_missing", "Attribute 'generationDateAndTime' is missing");
    }

    SECTION("Unused Definitions")
    {
        validate_warning("warn/unit_unused", "Unit \"s\" is unused.");
        validate_warning("warn/type_unused", "Type definition \"MyType\" (line 4) is unused.");
    }
}

TEST_CASE("FMI 1.0 Model Description Passing Cases", "[fmi1][pass]")
{
    Certificate cert;
    Fmi1ModelDescriptionChecker checker;

    SECTION("FMI 1.0 ME Valid")
    {
        checker.validate("tests/data/fmi1/pass/TestME", cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("FMI 1.0 CS Valid")
    {
        checker.validate("tests/data/fmi1/pass/TestCS", cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("FMI 1.0 CS Tool Valid")
    {
        checker.validate("tests/data/fmi1/pass/TestCSTool", cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("FMI 1.0 Special Floats Valid")
    {
        checker.validate("tests/data/fmi1/pass/SpecialFloats", cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("FMI 1.0 Alias Negated Valid")
    {
        checker.validate("tests/data/fmi1/pass/AliasNegated", cert);
        CHECK_FALSE(has_fail(cert));
    }
}
