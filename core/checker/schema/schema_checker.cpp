#include "schema_checker.h"

#include "certificate.h"
#include "xml_utils.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>
#include <libxml/xmlschemas.h>
#include <libxml/xmlstring.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // IWYU pragma: keep
#endif

#if defined(__linux__) || defined(__EMSCRIPTEN__) || defined(__APPLE__)
#include <sys/types.h> // IWYU pragma: keep
#include <unistd.h>
#endif

#ifdef __linux__
#include <linux/limits.h>
#include <string>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

void SchemaCheckerBase::validate(const std::filesystem::path& path, Certificate& cert)
{
    bool is_valid = true;

    cert.printSubsectionHeader("SCHEMA VALIDATION");

    // Check if directory exists
    if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
    {
        std::cerr << "Directory does not exist: " << path << "\n";
        cert.printTestResult({"Directory Existence", TestStatus::FAIL, {"Directory does not exist: " + path.string()}});
        cert.printSubsectionSummary(false);
        return;
    }

    // Get rules from derived class
    auto rules = getXmlRules(path);

    // Validate each XML file according to rules
    for (const auto& rule : rules)
    {
        const auto xml_path = path / rule.relative_path;

        // Check if file exists
        if (!std::filesystem::exists(xml_path))
        {
            if (rule.is_mandatory)
            {
                const TestResult test{rule.validation_name, TestStatus::FAIL, {"File not found (mandatory)"}};
                is_valid = false;
                cert.printTestResult(test);
            }
            continue;
        }

        // Validate UTF-8 encoding BEFORE schema validation
        if (!validateUtf8Encoding(xml_path, rule.validation_name, cert))
        {
            is_valid = false;
            continue; // Skip schema validation if encoding is wrong
        }

        // Find schema
        const auto schema_path = findSchemaPath(rule.schema_filename);

        if (schema_path.empty() || !std::filesystem::exists(schema_path))
        {
            const TestResult test{
                rule.validation_name, TestStatus::FAIL, {"Schema " + rule.schema_filename + " not found"}};
            is_valid = false;
            cert.printTestResult(test);
            continue;
        }

        // Validate
        validateXmlFile(xml_path, schema_path, rule.validation_name, cert);
    }

    cert.printSubsectionSummary(is_valid);
}

std::optional<std::string> SchemaCheckerBase::extractVersionFromXml(const std::filesystem::path& xml_path,
                                                                    const std::string& root_element,
                                                                    const std::string& version_attribute)
{
    xmlDocPtr doc = readXmlFile(xml_path);
    if (!doc)
        return std::nullopt;

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root)
    {
        xmlFreeDoc(doc);
        return std::nullopt;
    }

    // Check if root element name matches
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    if (xmlStrcmp(root->name, reinterpret_cast<const xmlChar*>(root_element.c_str())) != 0)
    {
        xmlFreeDoc(doc);
        return std::nullopt;
    }

    // Extract version attribute
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    xmlChar* version = xmlGetProp(root, reinterpret_cast<const xmlChar*>(version_attribute.c_str()));
    if (!version)
    {
        xmlFreeDoc(doc);
        return std::nullopt;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    std::string version_str(reinterpret_cast<char*>(version));
    xmlFree(version);
    xmlFreeDoc(doc);

    return version_str;
}

bool SchemaCheckerBase::validateUtf8Encoding(const std::filesystem::path& xml_path, const std::string& validation_name,
                                             Certificate& cert)
{
    TestResult test{validation_name + " (XML Version, Encoding)", TestStatus::PASS, {}};

    std::ifstream file(xml_path, std::ios::binary);
    if (!file.is_open())
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Failed to open file for encoding check.");
        cert.printTestResult(test);
        return false;
    }

    // Read first 512 bytes (should be enough for XML declaration)
    constexpr size_t XML_DECL_BUFFER_SIZE = 512;
    std::vector<char> buffer(XML_DECL_BUFFER_SIZE);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const size_t bytes_read = static_cast<size_t>(file.gcount());
    file.close();

    if (bytes_read == 0)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("File is empty.");
        cert.printTestResult(test);
        return false;
    }

    // Convert to string for easier searching (first line should be ASCII-compatible)
    const std::string first_part(buffer.data(), bytes_read);

    // Find end of first line (either \n or \r\n or just end of declaration ?>)
    size_t line_end = first_part.find_first_of("\r\n");
    if (line_end == std::string::npos)
    {
        // If no newline, check if we at least have the closing ?>
        line_end = first_part.find("?>");
        if (line_end != std::string::npos)
            line_end += 2; // Include the ?>
    }

    std::string first_line = (line_end != std::string::npos) ? first_part.substr(0, line_end) : first_part;

    // Check if first line contains XML declaration
    if (first_line.find("<?xml") != 0)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Missing XML declaration on first line.");
        cert.printTestResult(test);
        return false;
    }

    // Check XML version
    const size_t version_pos = first_line.find("version");
    if (version_pos == std::string::npos)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("XML declaration missing version attribute.");
        cert.printTestResult(test);
        return false;
    }

    // Extract version value
    const size_t version_quote_start = first_line.find_first_of("\"'", version_pos);
    if (version_quote_start == std::string::npos)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Malformed version attribute.");
        cert.printTestResult(test);
        return false;
    }

    const char version_quote_char = first_line[version_quote_start];
    const size_t version_quote_end = first_line.find(version_quote_char, version_quote_start + 1);
    if (version_quote_end == std::string::npos)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Malformed version attribute.");
        cert.printTestResult(test);
        return false;
    }

    const std::string version = first_line.substr(version_quote_start + 1, version_quote_end - version_quote_start - 1);

    // Check if version is 1.0
    if (version != "1.0")
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("XML version must be 1.0, found: " + version);
        cert.printTestResult(test);
        return false;
    }

    // Check if encoding is specified
    const size_t encoding_pos = first_line.find("encoding");
    if (encoding_pos == std::string::npos)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("XML declaration missing encoding attribute.");
        cert.printTestResult(test);
        return false;
    }

    // Extract encoding value
    const size_t quote_start = first_line.find_first_of("\"'", encoding_pos);
    if (quote_start == std::string::npos)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Malformed encoding attribute.");
        cert.printTestResult(test);
        return false;
    }

    const char quote_char = first_line[quote_start];
    const size_t quote_end = first_line.find(quote_char, quote_start + 1);
    if (quote_end == std::string::npos)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Malformed encoding attribute.");
        cert.printTestResult(test);
        return false;
    }

    std::string encoding = first_line.substr(quote_start + 1, quote_end - quote_start - 1);

    // Convert to uppercase for comparison
    std::transform(encoding.begin(), encoding.end(), encoding.begin(), ::toupper);

    // Check if encoding is UTF-8
    if (encoding != "UTF-8")
    {
        if (isUtf8Required())
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Encoding must be UTF-8, found: " + encoding);
            cert.printTestResult(test);
            return false;
        }

        test.status = TestStatus::WARNING;
        test.messages.push_back("Encoding is " + encoding + ". It is recommended to use UTF-8.");
        cert.printTestResult(test);
        return true;
    }

    // Validate actual file content is valid UTF-8
    if (!isValidUtf8File(xml_path))
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("File content is not valid UTF-8.");
        cert.printTestResult(test);
        return false;
    }

    cert.printTestResult(test);
    return true;
}

bool SchemaCheckerBase::isValidUtf8File(const std::filesystem::path& file_path)
{
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open())
        return false;

    constexpr size_t UTF8_CHECK_BUFFER_SIZE = 4096;
    std::vector<unsigned char> buffer(UTF8_CHECK_BUFFER_SIZE);

    while (file.read(reinterpret_cast<char*>(buffer.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                     static_cast<std::streamsize>(buffer.size())) ||
           file.gcount() > 0)
    {
        const size_t bytes_read = static_cast<size_t>(file.gcount());
        if (!isValidUtf8(buffer.data(), bytes_read))
            return false;
    }

    return true;
}

bool SchemaCheckerBase::isValidUtf8(const unsigned char* data, size_t length)
{
    size_t i = 0;
    constexpr unsigned char UTF8_1BYTE_MASK = 0x80;
    constexpr unsigned char UTF8_1BYTE_PREFIX = 0x00;
    constexpr unsigned char UTF8_2BYTE_MASK = 0xE0;
    constexpr unsigned char UTF8_2BYTE_PREFIX = 0xC0;
    constexpr unsigned char UTF8_3BYTE_MASK = 0xF0;
    constexpr unsigned char UTF8_3BYTE_PREFIX = 0xE0;
    constexpr unsigned char UTF8_4BYTE_MASK = 0xF8;
    constexpr unsigned char UTF8_4BYTE_PREFIX = 0xF0;
    constexpr unsigned char UTF8_CONTINUATION_MASK = 0xC0;
    constexpr unsigned char UTF8_CONTINUATION_PREFIX = 0x80;

    constexpr uint32_t UTF8_2BYTE_MIN = 0x80;
    constexpr uint32_t UTF8_3BYTE_MIN = 0x800;
    constexpr uint32_t UTF8_4BYTE_MIN = 0x10000;
    constexpr uint32_t UTF8_MAX_CODEPOINT = 0x10FFFF;
    constexpr uint32_t UTF16_SURROGATE_MIN = 0xD800;
    constexpr uint32_t UTF16_SURROGATE_MAX = 0xDFFF;

    constexpr int32_t BIT_SHIFT_6 = 6;
    constexpr int32_t BIT_SHIFT_12 = 12;
    constexpr int32_t BIT_SHIFT_18 = 18;

    constexpr unsigned char MASK_1F = 0x1F;
    constexpr unsigned char MASK_3F = 0x3F;
    constexpr unsigned char MASK_0F = 0x0F;
    constexpr unsigned char MASK_07 = 0x07;

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    while (i < length)
    {
        const unsigned char byte = data[i];
        size_t num_bytes = 0;

        // Determine number of bytes in this UTF-8 character
        if ((byte & UTF8_1BYTE_MASK) == UTF8_1BYTE_PREFIX)
        {
            // 1-byte character (ASCII)
            num_bytes = 1;
        }
        else if ((byte & UTF8_2BYTE_MASK) == UTF8_2BYTE_PREFIX)
        {
            // 2-byte character
            num_bytes = 2;
        }
        else if ((byte & UTF8_3BYTE_MASK) == UTF8_3BYTE_PREFIX)
        {
            // 3-byte character
            num_bytes = 3;
        }
        else if ((byte & UTF8_4BYTE_MASK) == UTF8_4BYTE_PREFIX)
        {
            // 4-byte character
            num_bytes = 4;
        }
        else
        {
            // Invalid UTF-8 start byte
            return false;
        }

        // Check if we have enough bytes
        if (i + num_bytes > length)
            return false;

        // Validate continuation bytes
        for (size_t j = 1; j < num_bytes; ++j)
            if ((data[i + j] & UTF8_CONTINUATION_MASK) != UTF8_CONTINUATION_PREFIX)
                return false;

        // Check for overlong encodings and invalid code points
        if (num_bytes == 2)
        {
            const uint32_t codepoint = ((data[i] & MASK_1F) << BIT_SHIFT_6) | (data[i + 1] & MASK_3F);
            if (codepoint < UTF8_2BYTE_MIN)
                return false; // Overlong encoding
        }
        else if (num_bytes == 3)
        {
            const uint32_t codepoint = ((data[i] & MASK_0F) << BIT_SHIFT_12) |
                                       ((data[i + 1] & MASK_3F) << BIT_SHIFT_6) | (data[i + 2] & MASK_3F);
            if (codepoint < UTF8_3BYTE_MIN)
                return false; // Overlong encoding
            if (codepoint >= UTF16_SURROGATE_MIN && codepoint <= UTF16_SURROGATE_MAX)
                return false; // UTF-16 surrogates not allowed in UTF-8
        }
        else if (num_bytes == 4)
        {
            const uint32_t codepoint = ((data[i] & MASK_07) << BIT_SHIFT_18) |
                                       ((data[i + 1] & MASK_3F) << BIT_SHIFT_12) |
                                       ((data[i + 2] & MASK_3F) << BIT_SHIFT_6) | (data[i + 3] & MASK_3F);
            if (codepoint < UTF8_4BYTE_MIN)
                return false; // Overlong encoding
            if (codepoint > UTF8_MAX_CODEPOINT)
                return false; // Beyond valid Unicode range
        }

        i += num_bytes;
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    return true;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::filesystem::path SchemaCheckerBase::findSchemaPath(const std::string& schema_filename,
                                                        const std::string& version_override) const
{
    std::filesystem::path bin_dir;

#ifdef _WIN32
    std::array<char, 260> path{};
    // NOLINTNEXTLINE(misc-include-cleaner)
    auto length = GetModuleFileNameA(nullptr, path.data(), static_cast<unsigned long>(path.size()));
    if (length > 0 && length < path.size())
        bin_dir = std::filesystem::path(path.data()).parent_path();

#elif defined(__linux__)
    std::array<char, PATH_MAX> path{};
    const ssize_t len = readlink("/proc/self/exe", path.data(), path.size() - 1);
    if (len != -1)
    {
        path[static_cast<size_t>(len)] = '\0';
        bin_dir = std::filesystem::path(path.data()).parent_path();
    }
#elif defined(__EMSCRIPTEN__)
    // Under Emscripten, schemas are preloaded at /standard in the virtual filesystem.
    bin_dir = "/";
#endif

    if (bin_dir.empty())
        bin_dir = std::filesystem::current_path();

    std::string version = version_override;
    if (version.empty())
    {
        // Detect version from filename for FMI to handle backported schemas correctly
        if (schema_filename.starts_with("fmi3"))
            version = "3.0";
        else if (schema_filename.starts_with("fmi2"))
            version = "2.0";
        else
            version = getStandardVersion();
    }

    // Normalize version string to major.minor if it doesn't contain a slash (e.g. "2.0.1" -> "2.0")
    if (version.find('/') == std::string::npos)
    {
        const size_t first_dot = version.find('.');
        if (first_dot != std::string::npos)
        {
            const size_t second_dot = version.find('.', first_dot + 1);
            if (second_dot != std::string::npos)
                version = version.substr(0, second_dot);
        }
    }

    const std::filesystem::path relative_schema_path =
        std::filesystem::path("standard") / getStandardName() / version / "schema" / schema_filename;

    std::vector<std::filesystem::path> candidates;
    if (!bin_dir.empty())
        candidates.push_back(bin_dir / relative_schema_path);

    const auto cwd = std::filesystem::current_path();
    candidates.push_back(cwd / relative_schema_path);

#ifdef __EMSCRIPTEN__
    candidates.push_back(std::filesystem::path("/") / relative_schema_path);
#endif

    for (const auto& candidate : candidates)
    {
        if (std::filesystem::exists(candidate))
            return candidate;

            // Robustness: try access() in case std::filesystem::exists is flaky in WASM
#if defined(__EMSCRIPTEN__) || defined(__linux__) || defined(__APPLE__)
        if (access(candidate.string().c_str(), F_OK) == 0)
            return candidate;
#endif
    }

#ifdef __EMSCRIPTEN__
    std::cerr << "[ERROR] Schema not found: " << schema_filename << "\n";
    std::cerr << "  Standard Name: " << getStandardName() << ", Version: " << version << "\n";
    std::cerr << "  Current CWD: " << cwd.string() << "\n";
    std::cerr << "  Attempted paths:\n";
    for (const auto& candidate : candidates)
        std::cerr << "    - " << candidate.string() << "\n";

    // Debug: List /standard directory if it exists, otherwise list root
    if (std::filesystem::exists("/standard"))
    {
        std::cerr << "  Contents of /standard:\n";
        for (const auto& entry : std::filesystem::recursive_directory_iterator("/standard"))
            std::cerr << "    " << entry.path().string() << "\n";
    }
    else
    {
        std::cerr << "  /standard directory does NOT exist in VFS. Listing root /:\n";
        for (const auto& entry : std::filesystem::recursive_directory_iterator("/"))
            std::cerr << "    " << entry.path().string() << "\n";
    }
#endif

    return std::filesystem::path();
}

bool SchemaCheckerBase::hasElement(const std::filesystem::path& xml_path, const std::string& element_name)
{
    xmlDocPtr doc = readXmlFile(xml_path);
    if (!doc)
        return false;

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root)
    {
        xmlFreeDoc(doc);
        return false;
    }

    bool found = false;
    for (xmlNodePtr child = root->children; child != nullptr; child = child->next)
    {
        if (child->type == XML_ELEMENT_NODE &&
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            xmlStrcmp(child->name, reinterpret_cast<const xmlChar*>(element_name.c_str())) == 0)
        {
            found = true;
            break;
        }
    }

    xmlFreeDoc(doc);
    return found;
}

void SchemaCheckerBase::validateXmlFile(const std::filesystem::path& xml_path, const std::filesystem::path& schema_path,
                                        const std::string& validation_name, Certificate& cert)
{
    TestResult test{validation_name + " (XML Schema)", TestStatus::PASS, {}};

    // Initialize libxml2
    xmlInitParser();

    // Load schema
    xmlSchemaParserCtxtPtr parser_ctx = xmlSchemaNewParserCtxt(schema_path.string().c_str());
    if (!parser_ctx)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Failed to create schema parser context.");
        cert.printTestResult(test);
        xmlCleanupParser();
        return;
    }

    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    xmlSchemaSetParserErrors(parser_ctx, reinterpret_cast<xmlSchemaValidityErrorFunc>(errorCallback),
                             reinterpret_cast<xmlSchemaValidityWarningFunc>(warningCallback), &test);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

    xmlSchemaPtr schema = xmlSchemaParse(parser_ctx);
    xmlSchemaFreeParserCtxt(parser_ctx);

    if (!schema)
    {
        if (test.messages.empty())
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Failed to parse schema.");
        }
        cert.printTestResult(test);
        xmlCleanupParser();
        return;
    }

    // Create validation context
    xmlSchemaValidCtxtPtr valid_ctx = xmlSchemaNewValidCtxt(schema);
    if (!valid_ctx)
    {
        test.status = TestStatus::FAIL;
        test.messages.push_back("Failed to create validation context.");
        cert.printTestResult(test);
        xmlSchemaFree(schema);
        xmlCleanupParser();
        return;
    }

    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    xmlSchemaSetValidErrors(valid_ctx, reinterpret_cast<xmlSchemaValidityErrorFunc>(errorCallback),
                            reinterpret_cast<xmlSchemaValidityWarningFunc>(warningCallback), &test);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

    const std::int32_t valid_result = xmlSchemaValidateFile(valid_ctx, xml_path.string().c_str(), 0);

    if (valid_result != 0)
    {
        if (test.messages.empty())
        {
            test.status = TestStatus::FAIL;
            test.messages.push_back("Schema validation failed.");
        }
    }

    cert.printTestResult(test);

    // Cleanup
    xmlSchemaFreeValidCtxt(valid_ctx);
    xmlSchemaFree(schema);
    xmlCleanupParser();
}

// NOLINTNEXTLINE(cert-dcl50-cpp)
void SchemaCheckerBase::errorCallback(void* ctx, const char* msg, ...)
{
    auto* test = static_cast<TestResult*>(ctx);

    constexpr size_t ERROR_BUFFER_SIZE = 1024;
    std::array<char, ERROR_BUFFER_SIZE> buffer{};
    va_list args; // NOLINT(cppcoreguidelines-pro-type-vararg, cppcoreguidelines-init-variables)
    va_start(args, msg);
    const std::int32_t written = vsnprintf(buffer.data(), buffer.size(), msg, args);
    va_end(args);

    if (written < 0)
    {
        test->messages.push_back("Error formatting validation message.");
        test->status = TestStatus::FAIL;
        return;
    }

    std::string error(buffer.data());
    if (!error.empty() && error.back() == '\n')
        error.pop_back();

    if (!error.empty() && error.back() != '.')
        error += ".";

    test->messages.push_back(error);
    test->status = TestStatus::FAIL;
}

// NOLINTNEXTLINE(cert-dcl50-cpp)
void SchemaCheckerBase::warningCallback(void* ctx, const char* msg, ...)
{
    auto* test = static_cast<TestResult*>(ctx);

    constexpr size_t ERROR_BUFFER_SIZE = 1024;
    std::array<char, ERROR_BUFFER_SIZE> buffer{};
    va_list args; // NOLINT(cppcoreguidelines-pro-type-vararg, cppcoreguidelines-init-variables)
    va_start(args, msg);
    const std::int32_t written = vsnprintf(buffer.data(), buffer.size(), msg, args);
    va_end(args);

    if (written < 0)
    {
        test->messages.push_back("Warning: Error formatting validation message.");
        return;
    }

    std::string warning(buffer.data());
    if (!warning.empty() && warning.back() == '\n')
        warning.pop_back();

    if (!warning.empty() && warning.back() != '.')
        warning += ".";

    test->messages.push_back(warning);
    if (test->status == TestStatus::PASS)
        test->status = TestStatus::WARNING;
}
