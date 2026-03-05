#include "certificate.h"
#include "fmi2_model_description_checker.h"
#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("FMI 2.0 Model Description Failure Cases", "[fmi2][fail]")
{
    Fmi2ModelDescriptionChecker checker;

    auto validate_fail = [&](const std::string& path, const std::string& expected_error)
    {
        Certificate cert;
        checker.validate("tests/data/fmi2/fail/" + path, cert);
        INFO("Checking path: " << path);
        if (!has_error_with_text(cert, expected_error))
        {
            UNSCOPED_INFO("Expected error '" << expected_error << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.status == TestStatus::FAIL)
                {
                    UNSCOPED_INFO("  FAIL: " << res.test_name);
                    for (const auto& msg : res.messages)
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        REQUIRE(has_fail(cert));
        CHECK(has_error_with_text(cert, expected_error));
    };

    SECTION("File")
    {
        validate_fail("missing_file", "modelDescription.xml not found");
        validate_fail("malformed_xml", "Failed to parse modelDescription.xml");
    }

    SECTION("Metadata")
    {
        validate_fail("fmi_version_missing", "attribute is missing");
        validate_fail("fmi_version_empty", "attribute is empty");
        validate_fail("fmi_version_invalid", "is invalid (must be exactly \"2.0\")");
        validate_fail("fmi_version_patch", "is invalid (must be exactly \"2.0\")");

        validate_fail("model_name_missing", "modelName attribute is missing");
        validate_fail("model_name_empty", "modelName attribute is empty");

        validate_fail("guid_missing", "guid attribute is missing");
        validate_fail("guid_empty", "guid attribute is empty");
        validate_fail("guid_invalid", "does not match expected GUID format");

        validate_fail("date_invalid", "is out of range");
        validate_fail("date_future", "is in the future");
        validate_fail("date_format", "does not match ISO 8601 format");
    }

    SECTION("Model Identifier")
    {
        validate_fail("model_identifier_invalid", "cannot start with a digit");
        validate_fail("model_identifier_too_long", "is too long");
    }

    SECTION("Interfaces")
    {
        validate_fail("interface_none", "At least one interface must be implemented");
        validate_fail("model_identifier_invalid", "cannot start with a digit");
        validate_fail("model_identifier_too_long", "is too long");
    }

    SECTION("Unit definitions")
    {
        validate_fail("unit_duplicate", "is defined multiple times");
        validate_fail("unit_display_unit_duplicate", "is defined multiple times");
        validate_fail("unit_factor_nan", "factor value \"NaN\"");
        validate_fail("unit_offset_inf", "offset value \"INF\"");
    }

    SECTION("Type definitions")
    {
        validate_fail("type_duplicate", "is defined multiple times");
        validate_fail("type_max_min", "max (5) must be >= min (10)");
        validate_fail("type_min_nan", "min value \"NaN\"");
        validate_fail("enumeration_variable_no_type", "must have a declaredType attribute");
        validate_fail("enumeration_item_duplicate_value", "must be unique within the same enumeration");
        validate_fail("enumeration_item_duplicate_name", "has multiple items named \"A\"");
        validate_fail("enumeration_no_item", "must have at least one Item");
    }

    SECTION("Log categories")
    {
        validate_fail("log_category_duplicate", "is defined multiple times");
    }

    SECTION("DefaultExperiment")
    {
        validate_fail("exp_start_nan", "startTime");
        validate_fail("exp_stop_less_start", "must be greater than startTime");
        validate_fail("exp_tolerance_inf", "tolerance value \"INF\"");
    }

    SECTION("Vendor annotations")
    {
        validate_fail("vendor_annotation_duplicate", "is defined multiple times");
    }

    SECTION("Variable Names")
    {
        validate_fail("duplicate_name", "is not unique");
        validate_fail("naming_flat_empty", "is empty");
        validate_fail("naming_flat_tab", "contains illegal tab character");
        validate_fail("naming_flat_cr", "contains illegal carriage return");
        validate_fail("naming_flat_lf", "contains illegal line feed");
        validate_fail("naming_structured_empty", "is not a legal variable name");
        validate_fail("naming_structured_invalid", "is not a legal variable name");
        validate_fail("naming_structured_invalid_der", "is not a legal variable name");
        validate_fail("naming_structured_invalid_qname", "is not a legal variable name");
        validate_fail("naming_structured_invalid_indices", "is not a legal variable name");
        validate_fail("naming_structured_invalid_char", "is not a legal variable name");
        validate_fail("naming_structured_invalid_start", "is not a legal variable name");
        validate_fail("naming_structured_invalid_dot_start", "is not a legal variable name");
    }

    SECTION("Variability")
    {
        validate_fail("variability_continuous_integer", "cannot have variability \"continuous\"");
        validate_fail("variability_continuous_boolean", "cannot have variability \"continuous\"");
        validate_fail("variability_continuous_string", "cannot have variability \"continuous\"");
        validate_fail("parameter_continuous", "Parameters must be \"fixed\" or \"tunable\"");
        validate_fail("independent_variability", "must have variability=\"continuous\"");
        validate_fail("multiple_set_non_input", "has 'canHandleMultipleSetPerTimeInstant' but causality is 'output'");
        validate_fail("multiple_set_cs_only", "not allowed for Co-Simulation only FMUs");
    }

    SECTION("Initial/Start Values")
    {
        validate_fail("start_illegal_calculated", "has initial=\"calculated\" but provides a start value");
        validate_fail("start_illegal_independent", "has causality=\"independent\" but provides a start value");
        validate_fail("start_missing", "must have a start value");
        validate_fail("combination_illegal", "has illegal combination");
        validate_fail("combination_illegal_parameter_continuous", "has illegal combination");
        validate_fail("combination_illegal_input_initial", "has illegal combination");

        validate_fail("independent_multiple", "Found multiple");
        validate_fail("independent_non_real", "must be of type \"Real\"");
        validate_fail("independent_with_start", "not allowed to have a \"start\" attribute");
        validate_fail("independent_with_initial", "not allowed to define \"initial\"");
    }

    SECTION("Aliases")
    {
        validate_fail("alias_conflicting_start", "At most one variable in an alias set");
        validate_fail("alias_inconsistent_unit", "All variables in an alias set");
        validate_fail("alias_constant_conflicting_start", "have different start values");
    }

    SECTION("References")
    {
        validate_fail("ref_type_undef", "references undefined type");
        validate_fail("ref_unit_undef", "references undefined unit");
        validate_fail("ref_display_unit_undef", "is not defined for unit");
    }

    SECTION("Bounds")
    {
        validate_fail("bounds_max_min", "max (5) must be >= min (10)");
        validate_fail("bounds_start_min", "start (5) must be >= min (10)");
        validate_fail("bounds_invalid_numeric", "Failed to parse numeric value");
        validate_fail("start_nan", "is NaN");
        validate_fail("nominal_inf", "nominal value \"INF\" is Infinity");
        validate_fail("nominal_neg_inf", "nominal value \"-inf\" is Infinity");
    }

    SECTION("Structure")
    {
        validate_fail("structure_output_missing", "ModelStructure/Outputs must have exactly one entry");
        validate_fail("structure_output_missing_one", "are missing from ModelStructure/Outputs: v2");
        validate_fail("structure_output_duplicate", "is listed multiple times in ModelStructure/Outputs");
        validate_fail("structure_output_extra",
                      "listed in ModelStructure/Outputs but does not have causality=\"output\"");
        validate_fail("structure_derivative_no_attr", "must have the \"derivative\" attribute");
        validate_fail("structure_derivative_missing", "must have exactly one entry");
        validate_fail("structure_derivative_duplicate", "is listed multiple times");
        validate_fail("structure_initial_unknowns_not_ordered", "ordered according to their ScalarVariable index");
        validate_fail("structure_dependencies_not_ordered", "ordered according to magnitude");
        validate_fail("structure_dependencies_kind_mismatch", "have the same number of list elements");
        validate_fail("structure_initial_unknowns_mismatch", "does not contain the expected set of variables");
        validate_fail("structure_initial_unknowns_state_approx", "der_x");
        validate_fail("structure_dependencies_kind_invalid_initial", "is not allowed in InitialUnknowns");
        validate_fail("structure_dependencies_kind_non_real", "only allowed for Real variables");
        validate_fail("derivative_index_out_of_range", "referencing index 99 which does not exist");
        validate_fail("derivative_non_real", "Continuous-time state \"x\" (line 5) must be of type Real");
        validate_fail("reinit_non_state", "but is not a continuous-time state");
        validate_fail("reinit_cs_only", "not allowed for Co-Simulation only FMUs");
        validate_fail("derivative_variability", "must have variability=\"continuous\"");
    }
}

TEST_CASE("FMI 2.0 Model Description Warning Cases", "[fmi2][warn]")
{
    Fmi2ModelDescriptionChecker checker;

    auto validate_warning = [&](const std::string& path, const std::string& expected_warning)
    {
        Certificate cert;
        checker.validate("tests/data/fmi2/warn/" + path, cert);
        INFO("Checking path: " << path);
        if (!has_warning_with_text(cert, expected_warning))
        {
            UNSCOPED_INFO("Expected warning '" << expected_warning << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.status == TestStatus::WARNING)
                {
                    UNSCOPED_INFO("  WARN: " << res.test_name);
                    for (const auto& msg : res.messages)
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        REQUIRE(has_warning(cert));
        CHECK(has_warning_with_text(cert, expected_warning));
    };

    SECTION("Metadata")
    {
        validate_warning("author_missing",
                         "Attribute 'author' is missing. It is recommended to provide the name of the author.");
        validate_warning("author_empty", "Attribute 'author' is empty.");
        validate_warning("generation_tool_missing", "Attribute 'generationTool' is missing. It is recommended to "
                                                    "provide the name of the tool that generated the model.");
        validate_warning("generation_tool_empty", "Attribute 'generationTool' is empty.");
        validate_warning("license_missing", "Attribute 'license' is missing. It is recommended to specify a license "
                                            "(e.g., 'BSD', 'MIT', 'Proprietary').");
        validate_warning("license_empty", "Attribute 'license' is empty.");
        validate_warning("copyright_missing",
                         "Attribute 'copyright' is missing. It is recommended to provide a copyright notice.");
        validate_warning("copyright_empty", "Attribute 'copyright' is empty.");
        validate_warning("copyright_format_no_symbol", "should begin with ©, 'Copyright', or 'Copr.'");
        validate_warning("copyright_format_no_year", "should include the year of publication");
        validate_warning("copyright_format_no_holder", "should include the name of the copyright holder");
        validate_warning(
            "model_version_missing",
            "Attribute 'version' is missing. It is recommended to provide a version number for the model.");
        validate_warning("model_version_empty", "Attribute 'version' is empty.");
        validate_warning("generation_date_and_time_missing", "Attribute 'generationDateAndTime' is missing. It is "
                                                             "recommended to provide a generation timestamp for "
                                                             "traceability.");
        validate_warning("generation_date_and_time_empty", "Attribute 'generationDateAndTime' is empty.");
        validate_warning("generation_date_and_time_old", "is before the 2.0 standard release (2014)");
        validate_warning("generation_date_and_time_too_old", "is before the 2.0 standard release (2014)");
    }

    SECTION("Interfaces")
    {
        validate_warning("model_identifier_long", "longer than recommended");
    }

    SECTION("Unit definitions")
    {
        validate_warning("unit_unused", "Unit \"s\" is unused");
    }

    SECTION("Type definitions")
    {
        validate_warning("type_unused", "Type definition \"UnusedType\" (line 4) is unused.");
    }
}

TEST_CASE("FMI 2.0 Model Description Passing Cases", "[fmi2][pass]")
{
    Fmi2ModelDescriptionChecker checker;

    SECTION("FMI 2.0 Valid")
    {
        Certificate cert;
        checker.validate("tests/data/fmi2/pass", cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("Structured Naming")
    {
        Certificate cert;
        checker.validate("tests/data/fmi2/pass/structured_naming", cert);
        CHECK_FALSE(has_fail(cert));
    }
}

TEST_CASE("FMI 2.0 Type and Unit Usage", "[fmi2][usage]")
{
    Fmi2ModelDescriptionChecker checker;
    Certificate cert;
    checker.validate("tests/data/fmi2/pass/type_usage", cert);

    // Should NOT have warning about unused definitions
    CHECK_FALSE(has_warning_with_text(cert, "Type definition \"Position\""));
    CHECK_FALSE(has_warning_with_text(cert, "Unit \"m\" is unused."));
}
