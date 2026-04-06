#include "certificate.h"
#include "file_utils.h"
#include "fmi1_directory_checker.h"
#include "fmi1_model_description_checker.h"
#include "fmi2_model_description_checker.h"
#include "fmi3_model_description_checker.h"
#include "test_helpers.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

TEST_CASE("FMI 1.0 Model Description Failure Cases", "[fmi1][fail]")
{
    Fmi1ModelDescriptionChecker checker;

    auto validate_fail =
        [&](const std::string& path, const std::string& expected_error, const std::string& original_path = "Test.fmu")
    {
        Certificate cert;
        checker.setOriginalPath(original_path);
        checker.validate("tests/data/fmi1/fail/" + path, cert);
        INFO("Checking path: " << file_utils::pathToUtf8(path));
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

        validate_fail("date_invalid", "out of range");
        validate_fail("date_future", "is in the future");
    }

    SECTION("Model Identifier")
    {
        validate_fail("model_identifier_invalid", "cannot start with a digit");
        validate_fail("model_identifier_too_long", "is too long");
    }

    SECTION("Aliases")
    {
        validate_fail("alias_inconsistent_unit", "All variables in an alias set (VR 1) must have the same unit");
        validate_fail("alias_inconsistent_type", "All variables in an alias set (VR 1) must have the same type");
        validate_fail("alias_inconsistent_start",
                      "All variables in an alias set (VR 1) must have equivalent start values");
        validate_fail("alias_inconsistent_start_negated",
                      "All variables in an alias set (VR 1) must have equivalent start values");
        validate_fail("alias_negated_boolean", "alias=\"negatedAlias\" but is of type Boolean");
        validate_fail("alias_multiple_noalias",
                      "All variables in an alias set (VR 1) must have exactly one base variable (noAlias)");
        validate_fail("alias_no_noalias",
                      "All variables in an alias set (VR 1) must have exactly one base variable (noAlias)");
        validate_fail("alias_inconsistent_variability_constant",
                      "All variables in an alias set (VR 1) must have the same variability");
    }

    SECTION("Implementation")
    {
        validate_fail("implementation/entry_point_missing_file",
                      R"(references missing file in FMU: "resources/non_existent.mdl")");
        validate_fail("implementation/file_missing_file",
                      R"(references missing file in FMU: "resources/missing_extra.txt")");
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
        checker.setOriginalPath("Test.fmu");
        checker.validate("tests/data/fmi1/" + path, cert);
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (!has_warning_with_text(cert, expected_warning))
        {
            UNSCOPED_INFO("Expected warning '" << expected_warning << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                std::string status_str =
                    (res.getStatus() == TestStatus::PASS ? "PASS"
                                                         : (res.getStatus() == TestStatus::FAIL ? "FAIL" : "WARN"));
                UNSCOPED_INFO("  " << status_str << ": " << res.getName());
                for (const auto& msg : res.getMessages())
                    UNSCOPED_INFO("    - " << msg);
            }
        }
        CHECK(has_warning_with_text(cert, expected_warning));
    };

    SECTION("Metadata")
    {
        validate_warning("warn/generation_date_too_old", "is before the 1.0 standard release (2010)");
        // Add dummy source to satisfy implementation check
        fs::create_directories("tests/data/fmi1/pass/SpecialFloats/sources");
        std::ofstream("tests/data/fmi1/pass/SpecialFloats/sources/test.c").close();
        validate_warning("warn/author_missing", "Providing the author name is recommended.");
        validate_warning("warn/author_empty", "The 'author' attribute is empty.");
        validate_warning(
            "warn/generation_tool_missing",
            "Providing the generationTool name is recommended. For manually created FMUs, use \"Handmade\".");
        validate_warning("warn/license_missing", "Providing a license is recommended.");
        validate_warning("warn/copyright_missing", "Providing a copyright notice is recommended.");
        validate_warning("warn/model_version_missing", "Providing a model version is recommended.");
        validate_warning("warn/generation_date_missing", "Providing 'generationDateAndTime' is recommended.");
    }

    SECTION("Unused Definitions")
    {
        validate_warning("warn/unit_unused", "Unit \"s\" (line 4) is unused.");
        validate_warning("warn/type_unused", "Type definition \"MyType\" (line 4) is unused.");
    }

    SECTION("Aliases")
    {
        Certificate cert;
        checker.setOriginalPath("Test.fmu");
        checker.validate("tests/data/fmi1/warn/alias_inconsistent_variability", cert);
        // This test case now passes the main alias check but still has a warning about variability consistency
        // which is what we want to test.
        CHECK(has_warning_with_text(cert, "All variables in an alias set (VR 1) should have the same variability"));
    }
}

TEST_CASE("FMI 1.0 Model Description Passing Cases", "[fmi1][pass]")
{
    Certificate cert;
    Fmi1ModelDescriptionChecker checker;

    SECTION("FMI 1.0 ME Valid")
    {
        Certificate cert_me;
        checker.validate("tests/data/fmi1/pass/TestME", cert_me);
        CHECK_FALSE(has_fail(cert_me));
    }

    SECTION("FMI 1.0 CS Valid")
    {
        Certificate cert_cs;
        checker.validate("tests/data/fmi1/pass/TestCS", cert_cs);
        CHECK_FALSE(has_fail(cert_cs));
    }

    SECTION("FMI 1.0 CS Tool Valid")
    {
        Certificate cert_cs_tool;
        checker.validate("tests/data/fmi1/pass/TestCSTool", cert_cs_tool);
        CHECK_FALSE(has_fail(cert_cs_tool));
    }

    SECTION("FMI 1.0 Special Floats Valid")
    {
        Certificate cert_floats;
        checker.validate("tests/data/fmi1/pass/SpecialFloats", cert_floats);
        CHECK_FALSE(has_fail(cert_floats));
    }

    SECTION("FMI 1.0 Alias Negated Valid")
    {
        Certificate cert_alias;
        checker.validate("tests/data/fmi1/pass/AliasNegated", cert_alias);
        CHECK_FALSE(has_fail(cert_alias));
    }

    SECTION("FMI 1.0 Alias Negated Integer Valid")
    {
        Certificate cert_alias_int;
        checker.validate("tests/data/fmi1/pass/AliasNegatedInteger", cert_alias_int);
        CHECK_FALSE(has_fail(cert_alias_int));
    }

    SECTION("FMI 1.0 Date Format (Date Only) Valid")
    {
        Certificate cert_date;
        checker.validate("tests/data/fmi1/pass/date_format", cert_date);
        CHECK_FALSE(has_fail(cert_date));
        CHECK_FALSE(has_error_with_text(cert_date, "Generation date and time"));
        CHECK_FALSE(has_warning_with_text(cert_date, "Generation date and time"));
    }
}

TEST_CASE("FMI 2.0 Model Description Failure Cases", "[fmi2][fail]")
{
    Fmi2ModelDescriptionChecker checker;

    auto validate_fail =
        [&](const std::string& path, const std::string& expected_error, const std::string& original_path = "Test.fmu")
    {
        Certificate cert;
        checker.setOriginalPath(original_path);
        checker.validate("tests/data/fmi2/fail/" + path, cert);
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (!has_error_with_text(cert, expected_error))
        {
            UNSCOPED_INFO("Expected error '" << expected_error << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::FAIL)
                {
                    UNSCOPED_INFO("  FAIL: " << res.getName());
                    for (const auto& msg : res.getMessages())
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
        validate_fail("fmi_version_missing", "fmiVersion attribute is missing");
        validate_fail("fmi_version_empty", "fmiVersion attribute is empty");
        validate_fail("fmi_version_invalid", "is invalid (must be exactly \"2.0\")");
        validate_fail("fmi_version_patch", "is invalid (must be exactly \"2.0\")");

        validate_fail("model_name_missing", "modelName attribute is missing");
        validate_fail("model_name_empty", "modelName attribute is empty");

        validate_fail("guid_missing", "guid attribute is missing");
        validate_fail("guid_empty", "guid attribute is empty");
        validate_fail("guid_invalid", "does not match expected GUID format");

        validate_fail("date_invalid", "out of range");
        validate_fail("date_future", "is in the future");
        validate_fail("date_format", "invalid or does not match ISO 8601 format");
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
        validate_fail(
            "alias_conflicting_start",
            "All variables in an alias set (VR 0) must have at most one non-constant variable with a start attribute");
        validate_fail("alias_inconsistent_unit", "All variables in an alias set (VR 0) must have the same unit");
        validate_fail("alias_constant_conflicting_start",
                      "All variables in an alias set (VR 1) must have the same start values if they are constant");
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
        validate_fail("structure_output_missing", "is missing a representative in ModelStructure/Outputs");
        validate_fail("structure_output_missing_one", "is missing a representative in ModelStructure/Outputs");
        validate_fail("structure_output_duplicate", "is already represented in ModelStructure/Outputs");
        validate_fail("structure_output_extra",
                      "listed in ModelStructure/Outputs but does not have causality=\"output\"");
        validate_fail("structure_derivative_no_attr", "must be of type Real and have a \"derivative\" attribute");
        // validate_fail("structure_derivative_missing", "must have exactly one entry"); // Removed: <Derivatives> is
        // not exhaustive anymore
        validate_fail("structure_derivative_duplicate", "is listed multiple times in ModelStructure/Derivatives");
        validate_fail("structure_initial_unknowns_not_ordered", "ordered according to their ScalarVariable index");
        validate_fail("structure_dependencies_not_ordered", "ordered according to magnitude");
        validate_fail("structure_dependencies_kind_mismatch", "have the same number of list elements");
        validate_fail("structure_initial_unknowns_mismatch",
                      "is missing a representative in ModelStructure/InitialUnknowns");
        validate_fail("structure_initial_unknowns_state_approx",
                      "is missing a representative in ModelStructure/InitialUnknowns");
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
        checker.setOriginalPath("Test.fmu");
        checker.validate("tests/data/fmi2/warn/" + path, cert);
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (!has_warning_with_text(cert, expected_warning))
        {
            UNSCOPED_INFO("Expected warning '" << expected_warning << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                std::string status_str =
                    (res.getStatus() == TestStatus::PASS ? "PASS"
                                                         : (res.getStatus() == TestStatus::FAIL ? "FAIL" : "WARN"));
                UNSCOPED_INFO("  " << status_str << ": " << res.getName());
                for (const auto& msg : res.getMessages())
                    UNSCOPED_INFO("    - " << msg);
            }
        }
        CHECK(has_warning_with_text(cert, expected_warning));
    };

    SECTION("Metadata")
    {
        validate_warning("author_and_copyright_missing", "Providing the author name is recommended.");
        validate_warning("author_and_copyright_missing", "Providing a copyright notice is recommended.");
        validate_warning("author_and_copyright_empty", "The 'author' attribute is empty.");
        validate_warning("author_and_copyright_empty", "The 'copyright' attribute is empty.");

        validate_warning(
            "generation_tool_missing",
            "Providing the generationTool name is recommended. For manually created FMUs, use \"Handmade\".");
        validate_warning("generation_tool_empty", "The 'generationTool' attribute is empty.");
        validate_warning("license_missing", "Providing a license is recommended.");
        validate_warning("license_empty", "The 'license' attribute is empty.");
        validate_warning("copyright_format_no_symbol", "should begin with ©, 'Copyright', or 'Copr.'");
        validate_warning("copyright_format_no_year", "should include the year of publication");
        validate_warning("copyright_format_no_holder", "should include the name of the copyright holder");
        validate_warning("model_version_missing", "Providing a model version is recommended.");
        validate_warning("model_version_empty", "Attribute 'version' is empty.");
        validate_warning("generation_date_and_time_missing", "Providing 'generationDateAndTime' is recommended.");
        validate_warning("generation_date_and_time_empty", "The 'generationDateAndTime' attribute is empty.");
        validate_warning("generation_date_and_time_old", "is before the 2.0 standard release (2014)");
        validate_warning("generation_date_and_time_too_old", "is before the 2.0 standard release (2014)");
    }

    SECTION("Interfaces")
    {
        validate_warning("model_identifier_long", "longer than recommended");
    }

    SECTION("Unit definitions")
    {
        validate_warning("unit_unused", "Unit \"s\" (line 4) is unused");
    }

    SECTION("Type definitions")
    {
        validate_warning("type_unused", "Type definition \"UnusedType\" (line 4) is unused.");
    }
}

TEST_CASE("FMI 2.0 Model Description Passing Cases", "[fmi2][pass]")
{
    Certificate cert;

    SECTION("FMI 2.0 Valid")
    {
        Fmi2ModelDescriptionChecker checker;
        checker.validate("tests/data/fmi2/pass", cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("Structured Naming")
    {
        Fmi2ModelDescriptionChecker checker;
        checker.validate("tests/data/fmi2/pass/structured_naming", cert);
        CHECK_FALSE(has_fail(cert));
    }
}

TEST_CASE("FMI 3.0 Model Description Failure Cases", "[fmi3][fail]")
{
    Fmi3ModelDescriptionChecker checker;

    auto validate_fail = [&](const std::string& path, const std::string& expected_error)
    {
        Certificate cert;
        checker.setOriginalPath("Test.fmu");
        checker.validate("tests/data/fmi3/fail/" + path, cert);
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (!has_error_with_text(cert, expected_error))
        {
            UNSCOPED_INFO("Expected error '" << expected_error << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::FAIL)
                {
                    UNSCOPED_INFO("  FAIL: " << res.getName());
                    for (const auto& msg : res.getMessages())
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
        validate_fail("fmi_version_missing", "fmiVersion attribute is missing");
        validate_fail("fmi_version_empty", "fmiVersion attribute is empty");

        validate_fail("instantiation_token_missing", "instantiationToken attribute is missing");
        validate_fail("instantiation_token_empty", "instantiationToken attribute is empty");

        validate_fail("date_invalid", "out of range");
        validate_fail("date_future", "is in the future");
        validate_fail("date_format", "invalid or does not match ISO 8601 format");
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
    }

    SECTION("Unit definitions")
    {
        validate_fail("unit_duplicate", "is defined multiple times");
    }

    SECTION("Type definitions")
    {
        validate_fail("enumeration_no_type", "must have a declaredType attribute");
    }

    SECTION("Variable Names")
    {
        validate_fail("duplicate_name", "is not unique");
        validate_fail("naming_flat_tab", "contains illegal tab character");
        validate_fail("naming_flat_cr", "contains illegal carriage return");
        validate_fail("naming_flat_lf", "contains illegal line feed");
        validate_fail("naming_structured_invalid", "is not a legal variable name");
        validate_fail("naming_structured_invalid_dot_start", "is not a legal variable name");
    }

    SECTION("Variability")
    {
        validate_fail("variability_continuous_integer", "cannot have variability \"continuous\"");
        validate_fail("variability_continuous_boolean", "cannot have variability \"continuous\"");
        validate_fail("variability_continuous_string", "cannot have variability \"continuous\"");
        validate_fail("parameter_discrete", "Parameters must be \"fixed\" or \"tunable\"");
        validate_fail("multiple_set_non_input", "must be 'input'");
    }

    SECTION("Initial/Start Values")
    {
        validate_fail("start_illegal_calculated", "has initial=\"calculated\" but provides a start value");
        validate_fail("start_missing", "must have a start value");
        validate_fail("combination_illegal", "has illegal combination");
        validate_fail("combination_illegal_parameter_continuous", "has illegal combination");
        validate_fail("combination_illegal_input_initial", "has illegal combination");

        validate_fail("independent_none", "Exactly one independent variable must be defined, found 0");
        validate_fail("independent_multiple", "Exactly one independent variable must be defined, found 2");
        validate_fail("independent_type", "must be of floating point type");
        validate_fail("independent_initial", "must not have an initial attribute");
        validate_fail("independent_start", "must not have a start attribute");

        validate_fail("clocked_independent", "Independent variable cannot have a clocks attribute");
        validate_fail("clock_type_bad_variability", "missing 'intervalDecimal' or 'intervalCounter'");
        validate_fail("clock_self", "Clock cannot reference itself");
        validate_fail("clock_ref_undef", "References non-existent clock");
        validate_fail("clock_ref_not_clock", "which is a Float64, not a Clock");
        validate_fail("clock_ref_invalid_vr", "Invalid clock reference");
        validate_fail("clocked_var_parameter", "Parameters (causality='parameter') cannot have a clocks attribute");
        validate_fail("clocked_var_causality", "Clocked variables must have causality 'input', 'output', or 'local'");
        validate_fail("clocked_var_variability", "Continuous variables cannot have a clocks attribute");

        validate_fail("array_start_count", "Expected either 3 values or 1 scalar value");
        validate_fail("array_start_count_string", "Expected either 3 values or 1 scalar value");
    }

    SECTION("Aliases")
    {
        validate_fail(
            "alias_multiple_non_local",
            "All variables in an alias set (VR 0) must have at most one variable with causality other than 'local'");
    }

    SECTION("References")
    {
        validate_fail("ref_type_undef", "references undefined type");
        validate_fail("ref_unit_undef", "references undefined unit");
        validate_fail("ref_display_unit_undef", "is not defined for unit");
    }

    SECTION("Bounds")
    {
        validate_fail("bounds_int8", "start (10) must be <= max (5)");
        validate_fail("bounds_max_min", "max (5) must be >= min (10)");
        validate_fail("bounds_start_min", "start (5) must be >= min (10)");
        validate_fail("bounds_invalid_numeric", "Failed to parse numeric value");
    }

    SECTION("Structure")
    {
        validate_fail("dim_sp_zero", "must be > 0");
        validate_fail("dim_both_start_vr", "must have either 'start' OR 'valueReference', not both");
        validate_fail("dim_none", "must have either 'start' or 'valueReference' attribute");
        validate_fail("dim_vr_undef", "references value reference 999 which is not a structural parameter");
        validate_fail("sp_type_invalid", "must be of type UInt64");

        validate_fail("structure_output_missing", "is missing a representative in ModelStructure/Output");
        validate_fail("structure_output_duplicate", "is listed multiple times in ModelStructure/Output");
        validate_fail("structure_output_extra",
                      "listed in ModelStructure/Output but does not have causality=\"output\"");
        validate_fail("structure_derivative_no_attr", "must have a \"derivative\" attribute");
        validate_fail("derivative_dimension_mismatch", "but has different dimensions");
        // validate_fail("structure_derivative_missing", "must have exactly one entry"); // Removed
        validate_fail("structure_derivative_duplicate",
                      "is listed multiple times in ModelStructure/ContinuousStateDerivative");
        validate_fail("structure_initial_unknowns_mismatch",
                      "missing a representative in ModelStructure/InitialUnknown");
        validate_fail("derivative_non_continuous", "must have variability=\"continuous\"");
        validate_fail("derivative_non_float", "must be Float32 or Float64");
        validate_fail("reinit_non_state", "is not a continuous-time state");
        validate_fail("reinit_non_float", "must be Float32 or Float64");
        validate_fail("structure_clocked_state_missing", "missing from ModelStructure/ClockedState");
        validate_fail("event_indicator_bad_causality", "does not have causality='local' or 'output'");
        validate_fail("event_indicator_bad_type", "is used as an event indicator but is of type Int32");
        validate_fail("event_indicator_bad_variability", "does not have variability='continuous'");
        validate_fail("structure_initial_unknown_duplicate",
                      "is listed multiple times in ModelStructure/InitialUnknown");
        validate_fail("structure_event_indicator_duplicate",
                      "is listed multiple times in ModelStructure/EventIndicator");

        validate_fail("structure_dependencies_missing", "has 'dependenciesKind' but 'dependencies' is missing");
        validate_fail("structure_dependencies_kind_mismatch", "has different number of elements in 'dependencies'");
        validate_fail("structure_dependencies_kind_invalid_initial", "has illegal dependencyKind 'fixed'");
        validate_fail("structure_dependencies_kind_non_float",
                      "has dependencyKind 'constant' but unknown is not a float type");
    }
}

TEST_CASE("FMI 3.0 Model Description Warning Cases", "[fmi3][warn]")
{
    Fmi3ModelDescriptionChecker checker;

    auto validate_warning = [&](const std::string& path, const std::string& expected_warning)
    {
        Certificate cert;
        checker.setOriginalPath("Test.fmu");
        checker.validate("tests/data/fmi3/warn/" + path, cert);
        INFO("Checking path: " << file_utils::pathToUtf8(path));
        if (!has_warning_with_text(cert, expected_warning))
        {
            UNSCOPED_INFO("Expected warning '" << expected_warning << "' not found in results:");
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::WARNING)
                {
                    UNSCOPED_INFO("  WARN: " << res.getName());
                    for (const auto& msg : res.getMessages())
                        UNSCOPED_INFO("    - " << msg);
                }
            }
        }
        REQUIRE(has_warning(cert));
        CHECK(has_warning_with_text(cert, expected_warning));
    };

    SECTION("Metadata")
    {
        validate_warning("author_and_copyright_missing", "Providing the author name is recommended.");
        validate_warning("author_and_copyright_missing", "Providing a copyright notice is recommended.");
        validate_warning("author_and_copyright_empty", "The 'author' attribute is empty.");
        validate_warning("author_and_copyright_empty", "The 'copyright' attribute is empty.");

        validate_warning(
            "metadata_missing",
            "Providing the generationTool name is recommended. For manually created FMUs, use \"Handmade\".");
        validate_warning("metadata_missing", "Providing a license is recommended.");

        validate_warning("model_version_missing", "Providing a model version is recommended.");
        validate_warning("model_version_empty", "Attribute 'version' is empty.");

        validate_warning("generation_date_and_time_missing", "Providing 'generationDateAndTime' is recommended.");
        validate_warning("generation_date_and_time_old", "is before the 3.0 standard release (2022)");
        validate_warning("generation_date_and_time_too_old", "is before the 3.0 standard release (2022)");

        validate_warning("copyright_format_no_symbol", "should begin with ©, 'Copyright', or 'Copr.'");
        validate_warning("copyright_format_no_year", "should include the year of publication");
        validate_warning("copyright_format_no_holder", "should include the name of the copyright holder");

        validate_warning("instantiation_token_no_guid", "does not match GUID format");
    }

    SECTION("Interfaces")
    {
        validate_warning("model_identifier_long", "longer than recommended");
    }

    SECTION("Unit definitions")
    {
        validate_warning("definitions_unused", "Unit \"s\" (line 5) is unused");
    }

    SECTION("Type definitions")
    {
        validate_warning("definitions_unused", "Type definition \"UnusedType\" (line 9) is unused");
    }

    SECTION("DefaultExperiment")
    {
    }
}

TEST_CASE("FMI 3.0 Model Description Passing Cases", "[fmi3][pass]")
{
    Certificate cert;
    Fmi3ModelDescriptionChecker checker;

    SECTION("FMI 3.0 Valid")
    {
        checker.validate("tests/data/fmi3/pass", cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("FMI 3.0 Patch Version")
    {
        checker.validate("tests/data/fmi3/pass/fmi_version_patch", cert);
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "version \"3.0.1\" is invalid (must be exactly \"3.0\")."));
    }

    SECTION("DefaultExperiment INF")
    {
        checker.validate("tests/data/fmi3/pass/stop_time_inf", cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("DefaultExperiment NaN")
    {
        checker.validate("tests/data/fmi3/pass/exp_nan", cert);
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("Unit Offset INF")
    {
        checker.validate("tests/data/fmi3/pass/unit_offset_inf", cert);
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

TEST_CASE("FMI 2.0 Source code detection when directory is missing", "[fmi2][fail]")
{
    Fmi2ModelDescriptionChecker checker;
    Certificate cert;
    checker.setOriginalPath("Test.fmu");
    checker.validate("tests/data/fmi2/fail/source_code_missing_dir", cert);

    auto summary = cert.getSummary();
    bool has_source_code = std::ranges::find(summary.fmu_types, "Source code") != summary.fmu_types.end();

    CHECK(has_source_code);
}

TEST_CASE("FMI 2.0 ModelStructure Alias and Partial Validation", "[fmi2][structure]")
{
    Fmi2ModelDescriptionChecker checker;

    auto validate = [&](const std::string& path)
    {
        Certificate cert;
        std::string full_path = "tests/data/fmi2/structure_tests/" + path;
        checker.validate(full_path, cert);
        if (has_fail(cert))
        {
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::FAIL)
                {
                    std::cout << "FAIL in " << path << ": " << res.getName() << "\n";
                    for (const auto& msg : res.getMessages())
                        std::cout << "  - " << msg << "\n";
                }
            }
        }
        return cert;
    };

    SECTION("Outputs with aliases")
    {
        Certificate cert = validate("outputs_alias_ok");
        CHECK_FALSE(has_fail(cert));

        cert = validate("outputs_alias_duplicate");
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "already represented in ModelStructure/Outputs"));

        cert = validate("outputs_missing_fail");
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "missing a representative in ModelStructure/Outputs"));
    }

    SECTION("Derivatives partial listing")
    {
        Certificate cert = validate("derivatives_partial_ok");
        CHECK_FALSE(has_fail(cert));

        cert = validate("derivatives_duplicate_vr_fail");
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "Value reference 1 is listed multiple times in ModelStructure/Derivatives"));
    }

    SECTION("InitialUnknowns with aliases")
    {
        Certificate cert = validate("initial_unknowns_alias_ok");
        CHECK_FALSE(has_fail(cert));

        cert = validate("initial_unknowns_missing_fail");
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(cert, "missing a representative in ModelStructure/InitialUnknowns"));
    }
}

TEST_CASE("FMI 3.0 ModelStructure Alias and Partial Validation", "[fmi3][structure]")
{
    Fmi3ModelDescriptionChecker checker;

    auto validate = [&](const std::string& path)
    {
        Certificate cert;
        std::string full_path = "tests/data/fmi3/structure_tests/" + path;
        checker.validate(full_path, cert);
        if (has_fail(cert))
        {
            for (const auto& res : cert.getResults())
            {
                if (res.getStatus() == TestStatus::FAIL)
                {
                    std::cout << "FAIL in " << path << ": " << res.getName() << "\n";
                    for (const auto& msg : res.getMessages())
                        std::cout << "  - " << msg << "\n";
                }
            }
        }
        return cert;
    };

    SECTION("Output with clocked variables")
    {
        Certificate cert = validate("output_clocked_excluded_ok");
        CHECK_FALSE(has_fail(cert));

        cert = validate("output_clocked_fail");
        CHECK(has_fail(cert));
        CHECK(has_error_with_text(
            cert, "is a clocked variable. Clocked variables must not be listed in ModelStructure/Output"));
    }

    SECTION("ContinuousStateDerivative partial listing")
    {
        Certificate cert = validate("derivatives_partial_ok");
        CHECK_FALSE(has_fail(cert));
    }

    SECTION("InitialUnknown with clocked variables")
    {
        Certificate cert = validate("initial_unknown_clocked_ok");
        CHECK_FALSE(has_fail(cert));
    }
}
