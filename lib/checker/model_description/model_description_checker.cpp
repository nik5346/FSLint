#include "model_description_checker.h"
#include "certificate.h"
#include "structured_name_parser.h"

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

void ModelDescriptionCheckerBase::validate(const std::filesystem::path& path, Certificate& cert)
{
    cert.printSubsectionHeader("MODEL DESCRIPTION VALIDATION");

    auto model_desc_path = path / "modelDescription.xml";

    if (!std::filesystem::exists(model_desc_path))
    {
        TestResult test{"Model Description File", TestStatus::FAIL, {"modelDescription.xml not found"}};
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    // Perform version-specific validation
    performVersionSpecificChecks(model_desc_path, cert);

    cert.printSubsectionSummary(true);
}

void ModelDescriptionCheckerBase::checkUniqueVariableNames(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Unique Variable Names", TestStatus::PASS, {}};

    std::set<std::string> seen_names;

    for (const auto& var : variables)
    {
        if (seen_names.count(var.name))
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable name \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") is not unique");
        }
        seen_names.insert(var.name);
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkUnits(const std::map<std::string, UnitDefinition>& units, Certificate& cert)
{
    TestResult test{"Unit Definitions", TestStatus::PASS, {}};

    // Check for duplicate unit names (shouldn't happen if map is used correctly)
    std::set<std::string> unit_names;
    for (const auto& [name, def] : units)
    {
        if (unit_names.count(name))
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Unit \"" + name + "\" is defined multiple times");
        }
        unit_names.insert(name);
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkVariableNamingConvention(const std::vector<Variable>& variables,
                                                                const std::string& convention, Certificate& cert)
{
    TestResult test{"Variable Naming Convention", TestStatus::PASS, {}};

    for (const auto& var : variables)
    {
        bool is_valid = true;

        if (convention == "flat")
        {
            // Check specific illegal characters for flat names
            if (var.name.find('\r') != std::string::npos)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") contains illegal carriage return character (U+000D)");
                is_valid = false;
            }
            if (var.name.find('\n') != std::string::npos)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") contains illegal line feed character (U+000A)");
                is_valid = false;
            }
            if (var.name.find('\t') != std::string::npos)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") contains illegal tab character (U+0009)");
                is_valid = false;
            }
        }
        else if (convention == "structured")
        {
            is_valid = StructuredNameParser::isValid(var.name);

            if (!is_valid)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("\"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") is not a legal variable name for naming convention \"structured\"");
            }
        }
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkGenerationDateAndTime(const std::optional<std::string>& generation_date_time,
                                                             Certificate& cert)
{
    TestResult test{"Generation Date and Time Format", TestStatus::PASS, {}};

    if (!generation_date_time.has_value())
    {
        // Optional attribute, so passing if not present
        cert.printTestResult(test);
        return;
    }

    const std::string& dt = *generation_date_time;

    // ISO 8601 formats supported:
    // Basic format: YYYY-MM-DDThh:mm:ssZ
    // With milliseconds: YYYY-MM-DDThh:mm:ss.sssZ
    // With timezone offset: YYYY-MM-DDThh:mm:ss+hh:mm or YYYY-MM-DDThh:mm:ss-hh:mm
    // The FMI standard recommends: YYYY-MM-DDThh:mm:ssZ
    std::regex datetime_pattern(R"(^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.(\d+))?(Z|[+-]\d{2}:\d{2})$)");
    std::smatch matches;

    if (!std::regex_match(dt, matches, datetime_pattern))
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Generation date and time \"" + dt +
                                "\" does not match ISO 8601 format (expected YYYY-MM-DDThh:mm:ssZ or similar)");
        cert.printTestResult(test);
        return;
    }

    // Validate date/time ranges
    int year = std::stoi(matches[1]);
    int month = std::stoi(matches[2]);
    int day = std::stoi(matches[3]);
    int hour = std::stoi(matches[4]);
    int minute = std::stoi(matches[5]);
    int second = std::stoi(matches[6]);
    std::string timezone = matches[8];

    if (month < 1 || month > 12)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Month value " + std::to_string(month) + " is out of range (1-12)");
    }

    if (day < 1 || day > 31)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Day value " + std::to_string(day) + " is out of range (1-31)");
    }

    if (hour > 23)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Hour value " + std::to_string(hour) + " is out of range (0-23)");
    }

    if (minute > 59)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Minute value " + std::to_string(minute) + " is out of range (0-59)");
    }

    if (second > 59)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Second value " + std::to_string(second) + " is out of range (0-59)");
    }

    // Check if the generation date is in the past and not unreasonably old
    if (test.status == TestStatus::PASS) // Only check if format validation passed
    {
        try
        {
            // Parse the datetime string into a time_point
            std::tm tm_time = {};
            tm_time.tm_year = year - 1900;
            tm_time.tm_mon = month - 1;
            tm_time.tm_mday = day;
            tm_time.tm_hour = hour;
            tm_time.tm_min = minute;
            tm_time.tm_sec = second;

            // Convert to time_t (UTC)
            std::time_t generation_time = std::mktime(&tm_time);

            // Adjust for timezone if not UTC
            if (timezone != "Z")
            {
                // Parse timezone offset (e.g., "+02:00" or "-05:00")
                std::regex tz_pattern(R"(([+-])(\d{2}):(\d{2}))");
                std::smatch tz_matches;
                if (std::regex_match(timezone, tz_matches, tz_pattern))
                {
                    int tz_sign = (tz_matches[1] == "+") ? 1 : -1;
                    int tz_hours = std::stoi(tz_matches[2]);
                    int tz_minutes = std::stoi(tz_matches[3]);

                    // Subtract the timezone offset to get UTC time
                    generation_time -= tz_sign * (tz_hours * 3600 + tz_minutes * 60);
                }
            }

            // Get current time
            auto now = std::chrono::system_clock::now();
            std::time_t current_time = std::chrono::system_clock::to_time_t(now);

            // Check if generation time is in the future
            if (generation_time > current_time)
            {
                test.status = TestStatus::FAIL;

                // Format current time for error message using platform-safe approach
                std::tm current_tm = {};
#ifdef _WIN32
                if (gmtime_s(&current_tm, &current_time) != 0)
                {
                    test.messages.push_back("Generation date and time \"" + dt + "\" is in the future");
                }
                else
#else
                if (gmtime_r(&current_time, &current_tm) == nullptr)
                {
                    test.messages.push_back("Generation date and time \"" + dt + "\" is in the future");
                }
                else
#endif
                {
                    std::ostringstream current_time_str;
                    current_time_str << std::put_time(&current_tm, "%Y-%m-%dT%H:%M:%SZ");
                    test.messages.push_back("Generation date and time \"" + dt +
                                            "\" is in the future (current time: " + current_time_str.str() + ")");
                }
            }

            // Check if generation time is unreasonably old (before FMI 1.0 release in 2010)
            // FMI 1.0 was released in 2010, so any date before 2010-01-01 is suspicious
            std::tm fmi_first_release = {};
            fmi_first_release.tm_year = 2010 - 1900;
            fmi_first_release.tm_mon = 0; // January
            fmi_first_release.tm_mday = 1;
            fmi_first_release.tm_hour = 0;
            fmi_first_release.tm_min = 0;
            fmi_first_release.tm_sec = 0;
            std::time_t fmi_release_time = std::mktime(&fmi_first_release);

            if (generation_time < fmi_release_time)
            {
                test.status = TestStatus::WARNING;
                test.messages.push_back("Generation date and time \"" + dt +
                                        "\" is before the first FMI standard release (2010). " +
                                        "This is unusual and may indicate an incorrect timestamp.");
            }
        }
        catch (const std::exception& e)
        {
            // If time parsing fails for some reason, issue a warning but don't fail
            test.status = TestStatus::WARNING;
            test.messages.push_back("Could not verify if generation date is in the past: " + std::string(e.what()));
        }
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkFmiVersion(const std::string& fmi_version, Certificate& cert)
{
    TestResult test{"FMI Version Format", TestStatus::PASS, {}};

    if (fmi_version.empty())
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("FMI version attribute is empty");
        cert.printTestResult(test);
        return;
    }

    // Version format: X.Y or X.Y-suffix (e.g., "2.0", "3.0", "3.0-alpha.2")
    // Note: Patch versions (X.Y.Z) should NOT be used - all patch releases use X.Y for compatibility
    std::regex version_pattern(R"(^(\d+)\.(\d+)(?:-([a-zA-Z0-9.-]+))?$)");

    if (!std::regex_match(fmi_version, version_pattern))
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back(
            "FMI version \"" + fmi_version +
            "\" does not match expected format (X.Y or X.Y-suffix, patch versions should be omitted)");
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkGuid(const std::string& guid, const std::string& attribute_name,
                                            Certificate& cert)
{
    TestResult test{"GUID/Instantiation Token Format", TestStatus::PASS, {}};

    if (guid.empty())
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back(attribute_name + " attribute is empty");
        cert.printTestResult(test);
        return;
    }

    // GUID format variations:
    // Standard: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
    std::regex guid_pattern(
        R"(^(\{)?[0-9a-fA-F]{8}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{4}-?[0-9a-fA-F]{12}(\})?$)");

    if (!std::regex_match(guid, guid_pattern))
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back(attribute_name + " \"" + guid +
                                "\" does not match expected GUID format ({xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx})");
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkModelVersion(const std::optional<std::string>& version, Certificate& cert)
{
    TestResult test{"Model Version Format", TestStatus::PASS, {}};

    if (!version.has_value())
    {
        // Optional attribute, so passing if not present
        cert.printTestResult(test);
        return;
    }

    const std::string& ver = *version;

    if (ver.empty())
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Model version attribute is empty");
        cert.printTestResult(test);
        return;
    }

    // Semantic versioning format: MAJOR.MINOR.PATCH or simpler versions like MAJOR.MINOR
    // Also allow optional pre-release and build metadata (e.g., 1.0.0-alpha+001)
    std::regex semver_pattern(R"(^(\d+)\.(\d+)(?:\.(\d+))?(?:-([0-9A-Za-z\-\.]+))?(?:\+([0-9A-Za-z\-\.]+))?$)");

    if (!std::regex_match(ver, semver_pattern))
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Model version \"" + ver +
                                "\" does not follow semantic versioning format (recommended: MAJOR.MINOR.PATCH)");
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkCopyright(const std::optional<std::string>& copyright, Certificate& cert)
{
    TestResult test{"Copyright Notice Format", TestStatus::PASS, {}};

    if (!copyright.has_value())
    {
        // Optional attribute, so passing if not present
        cert.printTestResult(test);
        return;
    }

    const std::string& cr = *copyright;

    if (cr.empty())
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Copyright attribute is empty");
        cert.printTestResult(test);
        return;
    }

    // Standard copyright notice format components:
    // 1. Copyright symbol (©), word "Copyright", or abbreviation "Copr."
    // 2. Year(s) of publication (can be a range like 2020-2024)
    // 3. Copyright holder name
    // 4. Optional: "All Rights Reserved" or similar rights statement

    // Check for copyright symbol, word, or abbreviation at the beginning
    bool has_copyright_indicator = false;
    std::string remaining = cr;

    // Check for various copyright indicators
    if (cr.find("©") != std::string::npos)
    {
        has_copyright_indicator = true;
    }
    else if (cr.find("(c)") != std::string::npos || cr.find("(C)") != std::string::npos)
    {
        has_copyright_indicator = true;
        test.status = TestStatus::WARNING;
        test.messages.push_back("Copyright notice uses (c) or (C) instead of the © symbol - consider using © for "
                                "international recognition");
    }
    else
    {
        // Check if it starts with "Copyright" or "Copr." (case-insensitive)
        std::string cr_lower = cr;
        std::transform(cr_lower.begin(), cr_lower.end(), cr_lower.begin(), ::tolower);

        if (cr_lower.find("copyright") == 0 || cr_lower.find("copr.") == 0)
            has_copyright_indicator = true;
    }

    if (!has_copyright_indicator)
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Copyright notice should begin with ©, 'Copyright', or 'Copr.'");
    }

    // Check for a year (4 digits) anywhere in the notice
    std::regex year_pattern(R"(\b(19|20)\d{2}\b)");
    std::smatch year_match;

    if (!std::regex_search(cr, year_match, year_pattern))
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Copyright notice should include the year of publication (e.g., 2024)");
    }

    // Check if there's some text that could be the copyright holder name
    // This is a simple heuristic: after removing symbols and years, there should be some text
    std::string name_check = cr;
    name_check = std::regex_replace(name_check, std::regex("[©(c)(C)]"), "");
    name_check =
        std::regex_replace(name_check, std::regex(R"(\b(copyright|copr\.?)\b)", std::regex_constants::icase), "");
    name_check = std::regex_replace(name_check, std::regex(R"(\b(19|20)\d{2}\b)"), "");
    name_check = std::regex_replace(
        name_check, std::regex(R"(\b(all rights reserved|some rights reserved)\b)", std::regex_constants::icase), "");
    name_check = std::regex_replace(name_check, std::regex(R"([.,\-:\s]+)"), "");

    if (name_check.empty())
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Copyright notice should include the name of the copyright holder");
    }

    // Provide an informational note about proper format if warnings were issued
    if (test.status == TestStatus::WARNING && test.messages.size() > 0)
    {
        test.messages.push_back(
            "Recommended format: © [Year] [Copyright Holder Name] or Copyright [Year] [Copyright Holder Name]");
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkNumberOfImplementedInterfaces(
    const std::map<std::string, std::string>& model_identifiers, Certificate& cert)
{
    TestResult test{"Number of Implemented Interfaces", TestStatus::FAIL, {}};

    if (!model_identifiers.empty())
        test.status = TestStatus::PASS;
    else
        test.messages.push_back("At least one interface must be implemented");

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkModelIdentifier(const std::string& model_identifier,
                                                       const std::string& interface_name, Certificate& cert)
{
    TestResult test{"Model Identifier Format for Interface \"" + interface_name + "\"", TestStatus::PASS, {}};

    if (model_identifier.empty())
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Model identifier attribute is empty");
        cert.printTestResult(test);
        return;
    }

    // Check if it's a valid C identifier using regex
    // Pattern: starts with letter or underscore, followed by any number of letters, digits, or underscores
    std::regex c_identifier_pattern("^[a-zA-Z_][a-zA-Z0-9_]*$");

    if (!std::regex_match(model_identifier, c_identifier_pattern))
    {
        test.status = TestStatus::FAIL;

        // Provide helpful error message
        char first_char = model_identifier[0];
        if (first_char >= '0' && first_char <= '9')
        {
            test.messages.push_back("Model identifier \"" + model_identifier + "\" cannot start with a digit");
        }
        else
        {
            test.messages.push_back(
                "Model identifier \"" + model_identifier +
                "\" contains invalid characters (only letters, digits, and underscores are allowed)");
        }
    }

    // Check length recommendations
    const size_t RECOMMENDED_MAX_LENGTH = 64;
    const size_t ABSOLUTE_MAX_LENGTH = 200;

    if (model_identifier.length() > ABSOLUTE_MAX_LENGTH)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Model identifier \"" + model_identifier + "\" is too long (" +
                                std::to_string(model_identifier.length()) + " characters). Maximum length is " +
                                std::to_string(ABSOLUTE_MAX_LENGTH) +
                                " characters to ensure cross-platform compatibility");
    }
    else if (model_identifier.length() > RECOMMENDED_MAX_LENGTH)
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Model identifier \"" + model_identifier + "\" is longer than recommended (" +
                                std::to_string(model_identifier.length()) + " characters). Consider keeping it under " +
                                std::to_string(RECOMMENDED_MAX_LENGTH) + " characters for better portability");
    }

    cert.printTestResult(test);
}

std::map<std::string, UnitDefinition> ModelDescriptionCheckerBase::extractUnitDefinitions(xmlDocPtr doc)
{
    std::map<std::string, UnitDefinition> units;

    // FMI uses UnitDefinitions/Unit for both FMI2 and FMI3
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//UnitDefinitions/Unit");
    if (!xpath_obj)
        return units;

    xmlNodeSetPtr nodes = xpath_obj->nodesetval;
    if (!nodes)
    {
        xmlXPathFreeObject(xpath_obj);
        return units;
    }

    for (int i = 0; i < nodes->nodeNr; ++i)
    {
        xmlNodePtr unit_node = nodes->nodeTab[i];
        UnitDefinition unit_def;

        // Get name attribute
        unit_def.name = getXmlAttribute(unit_node, "name").value_or("");

        if (unit_def.name.empty())
            continue;

        // Extract DisplayUnit children
        for (xmlNodePtr child = unit_node->children; child; child = child->next)
        {
            if (child->type != XML_ELEMENT_NODE)
                continue;

            std::string elem_name = reinterpret_cast<const char*>(child->name);

            if (elem_name == "DisplayUnit")
            {
                auto display_unit_name = getXmlAttribute(child, "name");
                if (display_unit_name)
                    unit_def.display_units.insert(*display_unit_name);
            }
        }

        units[unit_def.name] = unit_def;
    }

    xmlXPathFreeObject(xpath_obj);
    return units;
}

ModelDescriptionCheckerBase::EffectiveBounds
ModelDescriptionCheckerBase::getEffectiveBounds(const Variable& var,
                                                const std::map<std::string, TypeDefinition>& type_definitions)
{
    EffectiveBounds bounds;

    // Variable's own min/max takes precedence (override)
    if (var.min)
    {
        bounds.min = var.min;
    }
    else if (var.declared_type)
    {
        // Fall back to type definition's min
        auto it = type_definitions.find(*var.declared_type);
        if (it != type_definitions.end())
            bounds.min = it->second.min;
    }

    if (var.max)
    {
        bounds.max = var.max;
    }
    else if (var.declared_type)
    {
        // Fall back to type definition's max
        auto it = type_definitions.find(*var.declared_type);
        if (it != type_definitions.end())
            bounds.max = it->second.max;
    }

    return bounds;
}

std::optional<std::string> ModelDescriptionCheckerBase::getXmlAttribute(xmlNodePtr node, const std::string& attr_name)
{
    xmlChar* attr = xmlGetProp(node, reinterpret_cast<const xmlChar*>(attr_name.c_str()));
    if (!attr)
        return std::nullopt;

    std::string value(reinterpret_cast<char*>(attr));
    xmlFree(attr);
    return value;
}

xmlXPathObjectPtr ModelDescriptionCheckerBase::getXPathNodes(xmlDocPtr doc, const std::string& xpath)
{
    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    if (!context)
        return nullptr;

    xmlXPathObjectPtr result = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(xpath.c_str()), context);

    xmlXPathFreeContext(context);
    return result;
}

void ModelDescriptionCheckerBase::checkDefaultExperiment(xmlDocPtr doc, Certificate& cert)
{
    TestResult test{"Default Experiment", TestStatus::PASS, {}};

    // Get DefaultExperiment element
    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//DefaultExperiment");

    if (!xpath_obj || !xpath_obj->nodesetval || xpath_obj->nodesetval->nodeNr == 0)
    {
        // DefaultExperiment is optional, so this is not an error
        if (xpath_obj)
            xmlXPathFreeObject(xpath_obj);
        cert.printTestResult(test);
        return;
    }

    xmlNodePtr exp_node = xpath_obj->nodesetval->nodeTab[0];

    // Extract attributes
    auto start_time_str = getXmlAttribute(exp_node, "startTime");
    auto stop_time_str = getXmlAttribute(exp_node, "stopTime");
    auto tolerance_str = getXmlAttribute(exp_node, "tolerance");
    auto step_size_str = getXmlAttribute(exp_node, "stepSize");

    std::optional<double> start_time;
    std::optional<double> stop_time;
    std::optional<double> tolerance;
    std::optional<double> step_size;

    // Parse startTime
    if (start_time_str.has_value())
    {
        try
        {
            start_time = std::stod(*start_time_str);

            // Check for invalid values
            if (std::isnan(*start_time))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("startTime is NaN (not a number)");
            }
            else if (std::isinf(*start_time))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("startTime cannot be infinite");
            }
            else if (*start_time < 0.0)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("startTime (" + std::to_string(*start_time) + ") must be non-negative");
            }
        }
        catch (const std::exception&)
        {
            // Schema should have caught this, but just in case
            test.status = TestStatus::FAIL;
            test.messages.push_back("startTime \"" + *start_time_str + "\" is not a valid number");
        }
    }

    // Parse stopTime
    if (stop_time_str.has_value())
    {
        try
        {
            stop_time = std::stod(*stop_time_str);

            // Check for invalid values - NaN is invalid, but infinity is valid (means run indefinitely)
            if (std::isnan(*stop_time))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("stopTime is NaN (not a number)");
            }
            else if (!std::isinf(*stop_time) && *stop_time < 0.0)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("stopTime (" + std::to_string(*stop_time) + ") must be non-negative");
            }
        }
        catch (const std::exception&)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("stopTime \"" + *stop_time_str + "\" is not a valid number");
        }
    }

    // Check stopTime > startTime if both are present and finite
    if (start_time.has_value() && stop_time.has_value() && test.status == TestStatus::PASS)
    {
        // If stopTime is infinite, the comparison is automatically valid
        if (!std::isinf(*stop_time) && *stop_time <= *start_time)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("stopTime (" + std::to_string(*stop_time) + ") must be greater than startTime (" +
                                    std::to_string(*start_time) + ")");
        }
    }

    // Parse tolerance
    if (tolerance_str.has_value())
    {
        try
        {
            tolerance = std::stod(*tolerance_str);

            // Check for invalid values
            if (std::isnan(*tolerance))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("tolerance is NaN (not a number)");
            }
            else if (std::isinf(*tolerance))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("tolerance cannot be infinite");
            }
            else if (*tolerance <= 0.0)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("tolerance (" + std::to_string(*tolerance) + ") must be greater than 0");
            }
        }
        catch (const std::exception&)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("tolerance \"" + *tolerance_str + "\" is not a valid number");
        }
    }

    // Parse stepSize
    if (step_size_str.has_value())
    {
        try
        {
            step_size = std::stod(*step_size_str);

            // Check for invalid values
            if (std::isnan(*step_size))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("stepSize is NaN (not a number)");
            }
            else if (std::isinf(*step_size))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("stepSize cannot be infinite");
            }
            else if (*step_size <= 0.0)
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("stepSize (" + std::to_string(*step_size) + ") must be greater than 0");
            }
        }
        catch (const std::exception&)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("stepSize \"" + *step_size_str + "\" is not a valid number");
        }
    }

    xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkTypeAndUnitReferences(
    const std::vector<Variable>& variables, const std::map<std::string, TypeDefinition>& type_definitions,
    const std::map<std::string, UnitDefinition>& units, Certificate& cert)
{
    TestResult test{"Type and Unit References", TestStatus::PASS, {}};

    used_type_definitions.clear();
    used_units.clear();

    // Check Variable references
    for (const auto& var : variables)
    {
        // 1. Check declaredType
        if (var.declared_type.has_value())
        {
            if (type_definitions.find(*var.declared_type) == type_definitions.end())
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") references undefined type \"" + *var.declared_type + "\"");
            }
            else
            {
                used_type_definitions.insert(*var.declared_type);
            }
        }

        // 2. Check unit and displayUnit
        std::optional<std::string> unit_to_check = var.unit;
        if (!unit_to_check.has_value() && var.declared_type.has_value())
        {
            auto it = type_definitions.find(*var.declared_type);
            if (it != type_definitions.end())
                unit_to_check = it->second.unit;
        }

        if (unit_to_check.has_value())
        {
            if (units.find(*unit_to_check) == units.end())
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") references undefined unit \"" + *unit_to_check + "\"");
            }
            else
            {
                // Only mark as used if it was directly on the variable
                if (var.unit.has_value())
                    used_units.insert(*var.unit);

                // Check displayUnit if it exists on the variable
                if (var.display_unit.has_value())
                {
                    const auto& unit_def = units.at(*unit_to_check);
                    if (unit_def.display_units.find(*var.display_unit) == unit_def.display_units.end())
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("DisplayUnit \"" + *var.display_unit + "\" of variable \"" + var.name +
                                                "\" (line " + std::to_string(var.sourceline) +
                                                ") is not defined for unit \"" + *unit_to_check + "\"");
                    }
                }
            }
        }
    }

    // Check TypeDefinition references
    for (const auto& [name, type_def] : type_definitions)
    {
        if (type_def.unit.has_value())
        {
            if (units.find(*type_def.unit) == units.end())
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Type definition \"" + name + "\" (line " +
                                        std::to_string(type_def.sourceline) + ") references undefined unit \"" +
                                        *type_def.unit + "\"");
            }
            else
            {
                if (used_type_definitions.count(name))
                    used_units.insert(*type_def.unit);

                // Check displayUnit if it exists on the type definition
                if (type_def.display_unit.has_value())
                {
                    const auto& unit_def = units.at(*type_def.unit);
                    if (unit_def.display_units.find(*type_def.display_unit) == unit_def.display_units.end())
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("DisplayUnit \"" + *type_def.display_unit + "\" of type definition \"" +
                                                name + "\" (line " + std::to_string(type_def.sourceline) +
                                                ") is not defined for unit \"" + *type_def.unit + "\"");
                    }
                }
            }
        }
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkUnusedDefinitions(const std::map<std::string, TypeDefinition>& type_definitions,
                                                         const std::map<std::string, UnitDefinition>& units,
                                                         Certificate& cert)
{
    TestResult test{"Unused Definitions", TestStatus::PASS, {}};

    for (const auto& [name, type_def] : type_definitions)
    {
        if (used_type_definitions.find(name) == used_type_definitions.end())
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("Type definition \"" + name + "\" (line " + std::to_string(type_def.sourceline) +
                                    ") is unused");
        }
    }

    for (const auto& [name, unit_def] : units)
    {
        if (used_units.find(name) == used_units.end())
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("Unit \"" + name + "\" is unused");
        }
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkDerivativeReferences(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Derivative References", TestStatus::PASS, {}};

    // Build a map of value_reference -> Variable for quick lookup
    std::map<uint32_t, const Variable*> vr_to_variable;
    for (const auto& var : variables)
        if (var.value_reference.has_value())
            vr_to_variable[*var.value_reference] = &var;

    // Check each variable that has a derivative_of attribute
    for (const auto& var : variables)
    {
        if (var.derivative_of.has_value())
        {
            uint32_t derivative_of_vr = *var.derivative_of;

            // Check if the referenced variable exists
            auto it = vr_to_variable.find(derivative_of_vr);
            if (it == vr_to_variable.end())
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") has derivative attribute referencing value reference " +
                                        std::to_string(derivative_of_vr) + " which does not exist");
            }
            else
            {
                const Variable* referenced_var = it->second;

                // Additional validation: the derivative should be of a continuous variable
                // (This is implicit in FMI spec - derivatives are of continuous states)
                if (referenced_var->variability != "continuous")
                {
                    test.status = TestStatus::WARNING;
                    test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                            ") is derivative of \"" + referenced_var->name +
                                            "\" which has variability \"" + referenced_var->variability +
                                            "\" (expected \"continuous\")");
                }
            }
        }
    }

    cert.printTestResult(test);
}

// Add this method to model_description_checker.cpp (or fmi3_model_description_checker.cpp for FMI3-only)

void ModelDescriptionCheckerBase::checkDerivativeDimensions(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Derivative Dimension Matching", TestStatus::PASS, {}};

    // Build a map of value_reference -> Variable for quick lookup
    std::map<uint32_t, const Variable*> vr_to_variable;
    for (const auto& var : variables)
        if (var.value_reference.has_value())
            vr_to_variable[*var.value_reference] = &var;

    // Check each variable that has a derivative_of attribute
    for (const auto& var : variables)
    {
        if (var.derivative_of.has_value())
        {
            uint32_t derivative_of_vr = *var.derivative_of;

            // Find the state variable
            auto it = vr_to_variable.find(derivative_of_vr);
            if (it == vr_to_variable.end())
            {
                // This will be caught by checkDerivativeReferences, so skip here
                continue;
            }

            const Variable* state_var = it->second;

            // Compare dimensions
            bool dimensions_match = compareDimensions(var, *state_var);

            if (!dimensions_match)
            {
                test.status = TestStatus::FAIL;

                std::string derivative_dims = formatDimensions(var);
                std::string state_dims = formatDimensions(*state_var);

                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") is derivative of \"" + state_var->name + "\" (line " +
                                        std::to_string(state_var->sourceline) + ") but has different dimensions. " +
                                        "Derivative dimensions: " + derivative_dims + ", " +
                                        "State dimensions: " + state_dims);
            }
        }
    }

    cert.printTestResult(test);
}

// Helper function to compare dimensions between two variables
bool ModelDescriptionCheckerBase::compareDimensions(const Variable& var1, const Variable& var2)
{
    // If one is an array and the other is not, they don't match
    if (var1.dimensions.empty() != var2.dimensions.empty())
        return false;

    // Both are scalars (no dimensions)
    if (var1.dimensions.empty() && var2.dimensions.empty())
        return true;

    // Both are arrays - must have same number of dimensions
    if (var1.dimensions.size() != var2.dimensions.size())
        return false;

    // Compare each dimension
    for (size_t i = 0; i < var1.dimensions.size(); ++i)
    {
        const auto& dim1 = var1.dimensions[i];
        const auto& dim2 = var2.dimensions[i];

        // Case 1: Both have fixed start values - must be equal
        if (dim1.start.has_value() && dim2.start.has_value())
        {
            if (*dim1.start != *dim2.start)
                return false;
        }
        // Case 2: Both reference value references - must reference the same parameter
        else if (dim1.value_reference.has_value() && dim2.value_reference.has_value())
        {
            if (*dim1.value_reference != *dim2.value_reference)
                return false;
        }
        // Case 3: One has fixed start, other has reference - they don't match
        // (even if the reference evaluates to the same value, structurally they're different)
        else
        {
            return false;
        }
    }

    return true;
}

// Helper function to format dimensions for error messages
std::string ModelDescriptionCheckerBase::formatDimensions(const Variable& var)
{
    if (var.dimensions.empty())
        return "scalar";

    std::string result = "[";
    for (size_t i = 0; i < var.dimensions.size(); ++i)
    {
        if (i > 0)
            result += ", ";

        const auto& dim = var.dimensions[i];
        if (dim.start.has_value())
            result += std::to_string(*dim.start);
        else if (dim.value_reference.has_value())
            result += "vr:" + std::to_string(*dim.value_reference);
        else
            result += "?";
    }
    result += "]";

    return result;
}