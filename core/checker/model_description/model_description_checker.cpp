#include "model_description_checker.h"

#include "certificate.h"
#include "structured_name_parser.h"

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <time.h>
#include <vector>

void ModelDescriptionCheckerBase::validate(const std::filesystem::path& path, Certificate& cert)
{
    cert.printSubsectionHeader("MODEL DESCRIPTION VALIDATION");
    _fmu_root_path = path;

    auto model_desc_path = path / "modelDescription.xml";

    if (!std::filesystem::exists(model_desc_path))
    {
        const TestResult test{"Model Description File", TestStatus::FAIL, {"modelDescription.xml not found."}};
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    xmlDocPtr doc = xmlReadFile(model_desc_path.string().c_str(), nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc)
    {
        const TestResult test{"Parse Model Description", TestStatus::FAIL, {"Failed to parse modelDescription.xml."}};
        cert.printTestResult(test);
        cert.printSubsectionSummary(false);
        return;
    }

    // Extract common data
    xmlNodePtr root = xmlDocGetRootElement(doc);
    const auto metadata = extractMetadata(root);
    auto variables = extractVariables(doc);
    applyDefaultInitialValues(variables);
    const auto type_definitions = extractTypeDefinitions(doc);
    const auto units = extractUnitDefinitions(doc);

    // Run common validation checks
    checkFmiVersion(metadata.fmiVersion, cert);
    checkModelName(metadata.modelName, cert);
    checkGuid(metadata.guid, cert);
    checkGenerationDateAndTime(metadata.generationDateAndTime, cert);
    checkModelVersion(metadata.modelVersion, cert);
    checkCopyright(metadata.copyright, cert);
    checkLicense(metadata.license, cert);
    checkAuthor(metadata.author, cert);
    checkGenerationTool(metadata.generationTool, cert);
    checkVariableNamingConvention(variables, metadata.variableNamingConvention, cert);

    // Perform interface checks
    std::vector<std::string> interface_elements;
    if (metadata.fmiVersion && metadata.fmiVersion->starts_with("2."))
        interface_elements = {"CoSimulation", "ModelExchange"};
    else
        interface_elements = {"CoSimulation", "ModelExchange", "ScheduledExecution"};

    const auto model_identifiers = extractModelIdentifiers(doc, interface_elements);
    checkNumberOfImplementedInterfaces(model_identifiers, cert);
    for (const auto& [interface_name, model_id] : model_identifiers)
        checkModelIdentifier(model_id, interface_name, cert);

    checkUnits(doc, cert);
    checkTypeDefinitions(doc, cert);
    checkLogCategories(doc, cert);
    checkDefaultExperiment(doc, cert);
    checkAnnotations(doc, cert);

    checkUniqueVariableNames(variables, cert);
    checkTypeNameClashes(variables, type_definitions, cert);
    checkLegalVariability(variables, cert);
    checkRequiredStartValues(variables, cert);
    checkCausalityVariabilityInitialCombinations(variables, cert);
    checkIllegalStartValues(variables, cert);
    checkTypeAndUnitReferences(variables, type_definitions, units, cert);

    checkUnusedDefinitions(type_definitions, units, cert);
    checkMinMaxStartValues(variables, type_definitions, cert);

    // Perform version-specific validation
    performVersionSpecificChecks(doc, variables, type_definitions, units, cert);

    xmlFreeDoc(doc);
    xmlCleanupParser();

    cert.printSubsectionSummary(true);
}

void ModelDescriptionCheckerBase::checkUniqueVariableNames(const std::vector<Variable>& variables, Certificate& cert)
{
    TestResult test{"Unique Variable Names", TestStatus::PASS, {}};

    std::set<std::string> seen_names;

    for (const auto& var : variables)
    {
        if (seen_names.contains(var.name))
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Variable name \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") is not unique.");
        }
        seen_names.insert(var.name);
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkTypeNameClashes(const std::vector<Variable>& variables,
                                                       const std::map<std::string, TypeDefinition>& type_definitions,
                                                       Certificate& cert)
{
    TestResult test{"Type and Variable Name Clashes", TestStatus::PASS, {}};

    std::set<std::string> variable_names;
    for (const auto& var : variables)
        variable_names.insert(var.name);

    for (const auto& [name, type_def] : type_definitions)
    {
        if (variable_names.contains(name))
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Type definition name \"" + name + "\" (line " +
                                    std::to_string(type_def.sourceline) +
                                    ") must be different from all variable names.");
        }
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkModelName(const std::optional<std::string>& model_name, Certificate& cert)
{
    TestResult test{"Model Name Format", TestStatus::PASS, {}};

    if (!model_name.has_value())
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("modelName attribute is missing.");
        cert.printTestResult(test);
        return;
    }

    if (model_name->empty())
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("modelName attribute is empty.");
        cert.printTestResult(test);
        return;
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
            // For "flat" convention, "any name is allowed".
            // However, we still check for control characters as they are highly problematic for tools and reporting.
            if (var.name.find('\r') != std::string::npos)
            {
                test.status = TestStatus::FAIL;
                const std::string escaped_name = std::regex_replace(var.name, std::regex("\r"), "\\r");
                test.messages.push_back("Variable \"" + escaped_name + "\" (line " + std::to_string(var.sourceline) +
                                        ") contains illegal carriage return character (U+000D).");
                is_valid = false;
            }
            if (var.name.find('\n') != std::string::npos)
            {
                test.status = TestStatus::FAIL;
                const std::string escaped_name = std::regex_replace(var.name, std::regex("\n"), "\\n");
                test.messages.push_back("Variable \"" + escaped_name + "\" (line " + std::to_string(var.sourceline) +
                                        ") contains illegal line feed character (U+000A).");
                is_valid = false;
            }
            if (var.name.find('\t') != std::string::npos)
            {
                test.status = TestStatus::FAIL;
                const std::string escaped_name = std::regex_replace(var.name, std::regex("\t"), "\\t");
                test.messages.push_back("Variable \"" + escaped_name + "\" (line " + std::to_string(var.sourceline) +
                                        ") contains illegal tab character (U+0009).");
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
                                        ") is not a legal variable name for naming convention \"structured\".");
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
        test.status = TestStatus::WARNING;
        test.messages.push_back("Attribute 'generationDateAndTime' is missing. It is recommended to provide a "
                                "generation timestamp for traceability.");
        cert.printTestResult(test);
        return;
    }

    const std::string& dt = *generation_date_time;

    if (dt.empty())
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Attribute 'generationDateAndTime' is empty.");
        cert.printTestResult(test);
        return;
    }

    // ISO 8601 formats supported:
    // Basic format: YYYY-MM-DDThh:mm:ssZ
    // With milliseconds: YYYY-MM-DDThh:mm:ss.sssZ
    // With timezone offset: YYYY-MM-DDThh:mm:ss+hh:mm or YYYY-MM-DDThh:mm:ss-hh:mm
    // The FMI standard recommends: YYYY-MM-DDThh:mm:ssZ
    const std::regex datetime_pattern(
        R"(^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.(\d+))?(Z|[+-]\d{2}:\d{2})$)");
    std::smatch matches;

    if (!std::regex_match(dt, matches, datetime_pattern))
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Generation date and time \"" + dt +
                                "\" does not match ISO 8601 format (expected YYYY-MM-DDThh:mm:ssZ or similar).");
        cert.printTestResult(test);
        return;
    }

    // Validate date/time ranges
    constexpr int32_t MATCH_INDEX_YEAR = 1;
    constexpr int32_t MATCH_INDEX_MONTH = 2;
    constexpr int32_t MATCH_INDEX_DAY = 3;
    constexpr int32_t MATCH_INDEX_HOUR = 4;
    constexpr int32_t MATCH_INDEX_MINUTE = 5;
    constexpr int32_t MATCH_INDEX_SECOND = 6;
    constexpr int32_t MATCH_INDEX_TIMEZONE = 8;

    constexpr int32_t MIN_MONTH = 1;
    constexpr int32_t MAX_MONTH = 12;
    constexpr int32_t MIN_DAY = 1;
    constexpr int32_t MAX_DAY = 31;
    constexpr int32_t MAX_HOUR = 23;
    constexpr int32_t MAX_MINUTE = 59;
    constexpr int32_t MAX_SECOND = 59;

    const int32_t year = std::stoi(matches[MATCH_INDEX_YEAR]);
    const int32_t month = std::stoi(matches[MATCH_INDEX_MONTH]);
    const int32_t day = std::stoi(matches[MATCH_INDEX_DAY]);
    const int32_t hour = std::stoi(matches[MATCH_INDEX_HOUR]);
    const int32_t minute = std::stoi(matches[MATCH_INDEX_MINUTE]);
    const int32_t second = std::stoi(matches[MATCH_INDEX_SECOND]);
    const std::string timezone = matches[MATCH_INDEX_TIMEZONE];

    if (month < MIN_MONTH || month > MAX_MONTH)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Month value " + std::to_string(month) + " is out of range (1-12).");
    }

    if (day < MIN_DAY || day > MAX_DAY)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Day value " + std::to_string(day) + " is out of range (1-31).");
    }

    if (hour > MAX_HOUR)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Hour value " + std::to_string(hour) + " is out of range (0-23).");
    }

    if (minute > MAX_MINUTE)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Minute value " + std::to_string(minute) + " is out of range (0-59).");
    }

    if (second > MAX_SECOND)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Second value " + std::to_string(second) + " is out of range (0-59).");
    }

    // Check if the generation date is in the past and not unreasonably old
    if (test.status == TestStatus::PASS) // Only check if format validation passed
    {
        try
        {
            constexpr int32_t UNIX_EPOCH_YEAR = 1900;
            // Parse the datetime string into a time_point
            std::tm tm_time = {};
            tm_time.tm_year = year - UNIX_EPOCH_YEAR;
            tm_time.tm_mon = month - 1;
            tm_time.tm_mday = day;
            tm_time.tm_hour = hour;
            tm_time.tm_min = minute;
            tm_time.tm_sec = second;

            // Convert to time_t (UTC) using portable function
#ifdef _WIN32
            std::time_t generation_time = _mkgmtime(&tm_time);
#else
            std::time_t generation_time = timegm(&tm_time);
#endif

            // Adjust for timezone if not UTC
            if (timezone != "Z")
            {
                // Parse timezone offset (e.g., "+02:00" or "-05:00")
                const std::regex tz_pattern(R"(([+-])(\d{2}):(\d{2}))");
                std::smatch tz_matches;
                if (std::regex_match(timezone, tz_matches, tz_pattern))
                {
                    const int32_t tz_sign = (tz_matches[1] == "+") ? 1 : -1;
                    const int32_t tz_hours = std::stoi(tz_matches[2]);
                    const int32_t tz_minutes = std::stoi(tz_matches[3]);

                    // Subtract the timezone offset to get UTC time
                    constexpr int32_t SECONDS_PER_HOUR = 3600;
                    constexpr int32_t SECONDS_PER_MINUTE = 60;
                    generation_time -=
                        static_cast<std::time_t>(tz_sign) * (static_cast<std::time_t>(tz_hours) * SECONDS_PER_HOUR +
                                                             static_cast<std::time_t>(tz_minutes) * SECONDS_PER_MINUTE);
                }
            }

            // Get current time
            auto now = std::chrono::system_clock::now();
            const std::time_t current_time = std::chrono::system_clock::to_time_t(now);

            // Check if generation time is in the future
            if (generation_time > current_time)
            {
                test.status = TestStatus::FAIL;

                // Format current time for error message using platform-safe approach
                std::tm current_tm = {};
#ifdef _WIN32
                if (gmtime_s(&current_tm, &current_time) != 0)
                {
                    test.messages.push_back("Generation date and time \"" + dt + "\" is in the future.");
                }
                else
#else
                if (gmtime_r(&current_time, &current_tm) == nullptr)
                {
                    test.messages.push_back("Generation date and time \"" + dt + "\" is in the future.");
                }
                else
#endif
                {
                    std::ostringstream current_time_str;
                    current_time_str << std::put_time(&current_tm, "%Y-%m-%dT%H:%M:%SZ");
                    test.messages.push_back("Generation date and time \"" + dt +
                                            "\" is in the future (current time: " + current_time_str.str() + ").");
                }
            }

            // Check if generation time is unreasonably old (before FMI 1.0 release in 2010)
            // FMI 1.0 was released in 2010, so any date before 2010-01-01 is suspicious
            constexpr int32_t FMI_FIRST_RELEASE_YEAR = 2010;
            std::tm fmi_first_release = {};
            fmi_first_release.tm_year = FMI_FIRST_RELEASE_YEAR - UNIX_EPOCH_YEAR;
            fmi_first_release.tm_mon = 0; // January
            fmi_first_release.tm_mday = 1;
            fmi_first_release.tm_hour = 0;
            fmi_first_release.tm_min = 0;
            fmi_first_release.tm_sec = 0;
            const std::time_t fmi_release_time = std::mktime(&fmi_first_release);

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

void ModelDescriptionCheckerBase::checkFmiVersion(const std::optional<std::string>& fmi_version, Certificate& cert)
{
    TestResult test{"FMI Version Format", TestStatus::PASS, {}};

    if (!fmi_version.has_value())
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("FMI version attribute is missing.");
        cert.printTestResult(test);
        return;
    }

    if (fmi_version->empty())
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("FMI version attribute is empty.");
        cert.printTestResult(test);
        return;
    }

    validateFmiVersionValue(*fmi_version, test);

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkModelVersion(const std::optional<std::string>& version, Certificate& cert)
{
    TestResult test{"Model Version", TestStatus::PASS, {}};

    if (!version.has_value())
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Attribute 'version' is missing. It is recommended to provide a version number for "
                                "the model.");
    }
    else if (version->empty())
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Attribute 'version' is empty.");
    }
    else
    {
        // Semantic versioning format: MAJOR.MINOR.PATCH or simpler versions like MAJOR.MINOR
        const std::regex semver_pattern(
            R"(^(\d+)\.(\d+)(?:\.(\d+))?(?:-([0-9A-Za-z\-\.]+))?(?:\+([0-9A-Za-z\-\.]+))?$)");

        if (!std::regex_match(*version, semver_pattern))
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Model version \"" + *version +
                                    "\" does not follow semantic versioning format (recommended: MAJOR.MINOR.PATCH).");
        }
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkCopyright(const std::optional<std::string>& copyright, Certificate& cert)
{
    TestResult test{"Copyright", TestStatus::PASS, {}};

    if (!copyright.has_value())
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Attribute 'copyright' is missing. It is recommended to provide a copyright notice.");
    }
    else if (copyright->empty())
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Attribute 'copyright' is empty.");
    }
    else
    {
        const std::string& cr = *copyright;
        bool has_copyright_indicator = false;

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
            std::string cr_lower = cr;
            std::transform(cr_lower.begin(), cr_lower.end(), cr_lower.begin(), ::tolower);

            if (cr_lower.find("copyright") == 0 || cr_lower.find("copr.") == 0)
                has_copyright_indicator = true;
        }

        if (!has_copyright_indicator)
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Copyright notice should begin with ©, 'Copyright', or 'Copr.'.");
        }

        const std::regex year_pattern(R"(\b(19|20)\d{2}\b)");
        std::smatch year_match;

        if (!std::regex_search(cr, year_match, year_pattern))
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Copyright notice should include the year of publication (e.g., 2026).");
        }

        std::string name_check = cr;
        name_check = std::regex_replace(name_check, std::regex("[©(c)(C)]"), "");
        name_check =
            std::regex_replace(name_check, std::regex(R"(\b(copyright|copr\.?)\b)", std::regex_constants::icase), "");
        name_check = std::regex_replace(name_check, std::regex(R"(\b(19|20)\d{2}\b)"), "");
        name_check = std::regex_replace(
            name_check, std::regex(R"(\b(all rights reserved|some rights reserved)\b)", std::regex_constants::icase),
            "");
        name_check = std::regex_replace(name_check, std::regex(R"([.,\-:\s]+)"), "");

        if (name_check.empty())
        {
            test.status = TestStatus::WARNING;
            test.messages.push_back("Copyright notice should include the name of the copyright holder.");
        }

        if (test.status == TestStatus::WARNING && !test.messages.empty())
        {
            test.messages.push_back(
                "Recommended format: © [Year] [Copyright Holder Name] or Copyright [Year] [Copyright Holder Name]");
        }
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkLicense(const std::optional<std::string>& license, Certificate& cert)
{
    TestResult test{"License", TestStatus::PASS, {}};

    if (!license.has_value())
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back(
            "Attribute 'license' is missing. It is recommended to specify a license (e.g., 'BSD', 'MIT', "
            "'Proprietary').");
    }
    else if (license->empty())
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Attribute 'license' is empty.");
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkAuthor(const std::optional<std::string>& author, Certificate& cert)
{
    TestResult test{"Author", TestStatus::PASS, {}};

    if (!author.has_value())
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Attribute 'author' is missing. It is recommended to provide the name of the author.");
    }
    else if (author->empty())
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Attribute 'author' is empty.");
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkGenerationTool(const std::optional<std::string>& tool, Certificate& cert)
{
    TestResult test{"Generation Tool", TestStatus::PASS, {}};

    if (!tool.has_value())
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Attribute 'generationTool' is missing. It is recommended to provide the name of the "
                                "tool that generated the model.");
    }
    else if (tool->empty())
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Attribute 'generationTool' is empty.");
    }

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkLogCategories(xmlDocPtr doc, Certificate& cert)
{
    TestResult test{"Log Categories Uniqueness", TestStatus::PASS, {}};

    xmlXPathObjectPtr xpath_obj = getXPathNodes(doc, "//LogCategories/Category");
    if (!xpath_obj || !xpath_obj->nodesetval)
    {
        if (xpath_obj)
            xmlXPathFreeObject(xpath_obj);
        cert.printTestResult(test);
        return;
    }

    std::set<std::string> seen_names;
    for (int32_t i = 0; i < xpath_obj->nodesetval->nodeNr; ++i)
    {
        xmlNodePtr node = xpath_obj->nodesetval->nodeTab[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        auto name = getXmlAttribute(node, "name");
        if (name)
        {
            if (seen_names.contains(*name))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Log category \"" + *name + "\" (line " + std::to_string(node->line) +
                                        ") is defined multiple times.");
            }
            seen_names.insert(*name);
        }
    }

    xmlXPathFreeObject(xpath_obj);
    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkNumberOfImplementedInterfaces(
    const std::map<std::string, std::string>& model_identifiers, Certificate& cert)
{
    TestResult test{"Number of Implemented Interfaces", TestStatus::FAIL, {}};

    if (!model_identifiers.empty())
        test.status = TestStatus::PASS;
    else
        test.messages.push_back("At least one interface must be implemented.");

    cert.printTestResult(test);
}

void ModelDescriptionCheckerBase::checkModelIdentifier(const std::string& model_identifier,
                                                       const std::string& interface_name, Certificate& cert)
{
    TestResult test{"Model Identifier Format for Interface \"" + interface_name + "\"", TestStatus::PASS, {}};

    if (model_identifier.empty())
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Model identifier attribute is empty.");
        cert.printTestResult(test);
        return;
    }

    // Check if it's a valid C identifier using regex
    // Pattern: starts with letter or underscore, followed by any number of letters, digits, or underscores
    const std::regex c_identifier_pattern("^[a-zA-Z_][a-zA-Z0-9_]*$");

    if (!std::regex_match(model_identifier, c_identifier_pattern))
    {
        test.status = TestStatus::FAIL;

        // Provide helpful error message
        const char first_char = model_identifier[0];
        if (first_char >= '0' && first_char <= '9')
        {
            test.messages.push_back("Model identifier \"" + model_identifier + "\" cannot start with a digit.");
        }
        else
        {
            test.messages.push_back(
                "Model identifier \"" + model_identifier +
                "\" contains invalid characters (only letters, digits, and underscores are allowed).");
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
                                " characters to ensure cross-platform compatibility.");
    }
    else if (model_identifier.length() > RECOMMENDED_MAX_LENGTH)
    {
        test.status = TestStatus::WARNING;
        test.messages.push_back("Model identifier \"" + model_identifier + "\" is longer than recommended (" +
                                std::to_string(model_identifier.length()) + " characters). Consider keeping it under " +
                                std::to_string(RECOMMENDED_MAX_LENGTH) + " characters for better portability.");
    }

    cert.printTestResult(test);
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

std::map<std::string, std::string>
ModelDescriptionCheckerBase::extractModelIdentifiers(xmlDocPtr doc, const std::vector<std::string>& interface_elements)
{
    std::map<std::string, std::string> model_identifiers;

    for (const auto& elem : interface_elements)
    {
        xmlXPathObjectPtr xpath = getXPathNodes(doc, "//" + elem);
        if (xpath && xpath->nodesetval && xpath->nodesetval->nodeNr > 0)
        {
            auto model_id = getXmlAttribute(
                xpath->nodesetval->nodeTab[0], // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                "modelIdentifier");
            if (model_id.has_value())
                model_identifiers[elem] = *model_id;
        }
        if (xpath)
            xmlXPathFreeObject(xpath);
    }

    return model_identifiers;
}

std::optional<std::string> ModelDescriptionCheckerBase::getXmlAttribute(xmlNodePtr node, const std::string& attr_name)
{
    if (!node)
        return std::nullopt;

    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    xmlChar* attr = xmlGetProp(node, reinterpret_cast<const xmlChar*>(attr_name.c_str()));
    if (!attr)
        return std::nullopt;

    std::string value(reinterpret_cast<char*>(attr));
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

    xmlFree(attr);
    return value;
}

std::string ModelDescriptionCheckerBase::normalizeFloatString(const std::string& value)
{
    std::string s = value;
    // Remove leading/trailing whitespace
    s.erase(0, s.find_first_not_of(" \t\n\r"));
    const size_t last = s.find_last_not_of(" \t\n\r");
    if (last != std::string::npos)
        s.erase(last + 1);

    if (s.empty())
        return "";

    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

bool ModelDescriptionCheckerBase::isSpecialFloat(const std::string& value)
{
    const std::string lower = normalizeFloatString(value);

    if (lower.empty())
        return false;

    if (lower == "nan")
        return true;

    if (lower == "inf" || lower == "+inf" || lower == "-inf")
        return true;

    return false;
}

std::string ModelDescriptionCheckerBase::getSpecialFloatDescription(const std::string& value)
{
    const std::string lower = normalizeFloatString(value);

    if (lower.empty())
        return "NaN or Infinity";

    if (lower == "nan")
        return "NaN";

    if (lower == "inf" || lower == "+inf" || lower == "-inf")
        return "Infinity";

    return "NaN or Infinity";
}

xmlXPathObjectPtr ModelDescriptionCheckerBase::getXPathNodes(xmlDocPtr doc, const std::string& xpath)
{
    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    if (!context)
        return nullptr;

    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    xmlXPathObjectPtr result = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(xpath.c_str()), context);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

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

    xmlNodePtr exp_node = xpath_obj->nodesetval->nodeTab[0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    // Extract attributes
    auto start_time_str = getXmlAttribute(exp_node, "startTime");
    auto stop_time_str = getXmlAttribute(exp_node, "stopTime");
    auto tolerance_str = getXmlAttribute(exp_node, "tolerance");
    auto step_size_str = getXmlAttribute(exp_node, "stepSize");

    std::optional<double> start_time;
    std::optional<double> stop_time;
    std::optional<double> tolerance;
    std::optional<double> step_size;

    auto checkSpecial = [&](const std::optional<std::string>& val, const std::string& attr_name)
    {
        if (val && isSpecialFloat(*val))
            validateDefaultExperimentSpecialFloat(test, *val, attr_name);
    };

    // Parse startTime
    if (start_time_str.has_value())
    {
        checkSpecial(start_time_str, "startTime");
        try
        {
            start_time = std::stod(*start_time_str);

            // Check for invalid values (already checked for special values above, but std::stod handles them too)
            if (*start_time < 0.0 && !std::isnan(*start_time) && !std::isinf(*start_time))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("startTime (" + std::to_string(*start_time) + ") must be non-negative.");
            }
        }
        catch (const std::exception&)
        {
            // Schema should have caught this, but just in case
            test.status = TestStatus::FAIL;
            test.messages.push_back("startTime \"" + *start_time_str + "\" is not a valid number.");
        }
    }

    // Parse stopTime
    if (stop_time_str.has_value())
    {
        checkSpecial(stop_time_str, "stopTime");
        try
        {
            stop_time = std::stod(*stop_time_str);

            // Check for invalid values - NaN is invalid, but infinity is valid (means run indefinitely)
            if (*stop_time < 0.0 && !std::isnan(*stop_time) && !std::isinf(*stop_time))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("stopTime (" + std::to_string(*stop_time) + ") must be non-negative.");
            }
        }
        catch (const std::exception&)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("stopTime \"" + *stop_time_str + "\" is not a valid number.");
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
                                    std::to_string(*start_time) + ").");
        }
    }

    // Parse tolerance
    if (tolerance_str.has_value())
    {
        checkSpecial(tolerance_str, "tolerance");
        try
        {
            tolerance = std::stod(*tolerance_str);

            // Check for invalid values
            if (*tolerance <= 0.0 && !std::isnan(*tolerance) && !std::isinf(*tolerance))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("tolerance (" + std::to_string(*tolerance) + ") must be greater than 0.");
            }
        }
        catch (const std::exception&)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("tolerance \"" + *tolerance_str + "\" is not a valid number.");
        }
    }

    // Parse stepSize
    if (step_size_str.has_value())
    {
        checkSpecial(step_size_str, "stepSize");
        try
        {
            step_size = std::stod(*step_size_str);

            // Check for invalid values
            if (*step_size <= 0.0 && !std::isnan(*step_size) && !std::isinf(*step_size))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("stepSize (" + std::to_string(*step_size) + ") must be greater than 0.");
            }
        }
        catch (const std::exception&)
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("stepSize \"" + *step_size_str + "\" is not a valid number.");
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

    _used_type_definitions.clear();
    _used_units.clear();

    // Check Variable references
    for (const auto& var : variables)
    {
        // 1. Check declaredType
        if (var.declared_type.has_value())
        {
            if (!type_definitions.contains(*var.declared_type))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") references undefined type \"" + *var.declared_type + "\".");
            }
            else
            {
                _used_type_definitions.insert(*var.declared_type);
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
            if (!units.contains(*unit_to_check))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                        ") references undefined unit \"" + *unit_to_check + "\".");
            }
            else
            {
                // Only mark as used if it was directly on the variable
                if (var.unit.has_value())
                    _used_units.insert(*var.unit);

                // Check displayUnit if it exists on the variable
                if (var.display_unit.has_value())
                {
                    const auto& unit_def = units.at(*unit_to_check);
                    if (!unit_def.display_units.contains(*var.display_unit))
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("DisplayUnit \"" + *var.display_unit + "\" of variable \"" + var.name +
                                                "\" (line " + std::to_string(var.sourceline) +
                                                ") is not defined for unit \"" + *unit_to_check + "\".");
                    }
                }

                // Check relativeQuantity vs offset
                if (var.relative_quantity)
                {
                    const auto& unit_def = units.at(*unit_to_check);
                    if (unit_def.offset.has_value())
                    {
                        try
                        {
                            const double offset_val = std::stod(*unit_def.offset);
                            if (offset_val != 0.0)
                            {
                                if (test.status == TestStatus::PASS)
                                    test.status = TestStatus::WARNING;
                                test.messages.push_back(
                                    "Variable \"" + var.name + "\" (line " + std::to_string(var.sourceline) +
                                    ") has relativeQuantity=\"true\" but is associated with unit \"" + *unit_to_check +
                                    "\" which has a non-zero offset (" + *unit_def.offset + ").");
                            }
                        }
                        catch (const std::exception&)
                        {
                            // Ignore parsing errors for optional attributes
                        }
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
            if (!units.contains(*type_def.unit))
            {
                test.status = TestStatus::FAIL;
                test.messages.push_back("Type definition \"" + name + "\" (line " +
                                        std::to_string(type_def.sourceline) + ") references undefined unit \"" +
                                        *type_def.unit + "\".");
            }
            else
            {
                if (_used_type_definitions.contains(name))
                    _used_units.insert(*type_def.unit);

                // Check displayUnit if it exists on the type definition
                if (type_def.display_unit.has_value())
                {
                    const auto& unit_def = units.at(*type_def.unit);
                    if (!unit_def.display_units.contains(*type_def.display_unit))
                    {
                        test.status = TestStatus::FAIL;
                        test.messages.push_back("DisplayUnit \"" + *type_def.display_unit + "\" of type definition \"" +
                                                name + "\" (line " + std::to_string(type_def.sourceline) +
                                                ") is not defined for unit \"" + *type_def.unit + "\".");
                    }
                }

                // Check relativeQuantity vs offset
                if (type_def.relative_quantity)
                {
                    const auto& unit_def = units.at(*type_def.unit);
                    if (unit_def.offset.has_value())
                    {
                        try
                        {
                            const double offset_val = std::stod(*unit_def.offset);
                            if (offset_val != 0.0)
                            {
                                if (test.status == TestStatus::PASS)
                                    test.status = TestStatus::WARNING;
                                test.messages.push_back(
                                    "Type definition \"" + name + "\" (line " + std::to_string(type_def.sourceline) +
                                    ") has relativeQuantity=\"true\" but references unit \"" + *type_def.unit +
                                    "\" which has a non-zero offset (" + *unit_def.offset + ").");
                            }
                        }
                        catch (const std::exception&)
                        {
                            // Ignore parsing errors for optional attributes
                        }
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
        if (!_used_type_definitions.contains(name))
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("Type definition \"" + name + "\" (line " + std::to_string(type_def.sourceline) +
                                    ") is unused.");
        }
    }

    for (const auto& [name, unit_def] : units)
    {
        if (!_used_units.contains(name))
        {
            if (test.status == TestStatus::PASS)
                test.status = TestStatus::WARNING;
            test.messages.push_back("Unit \"" + name + "\" is unused.");
        }
    }

    cert.printTestResult(test);
}
