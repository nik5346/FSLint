#include "certificate.h"
#include "file_utils.h"

#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#ifdef _WIN32
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <stdio.h>
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif
#endif

namespace
{
constexpr const char* RED = "\x1b[31m";
constexpr const char* YELLOW = "\x1b[33m";
constexpr const char* RESET = "\x1b[0m";

std::string stripAnsi(const std::string& str)
{
    std::string result;
    bool in_escape = false;
    for (size_t i = 0; i < str.length(); ++i)
    {
        if (str[i] == '\x1b')
        {
            in_escape = true;
        }
        else if (in_escape)
        {
            if (str[i] == 'm')
                in_escape = false;
        }
        else
        {
            result += str[i];
        }
    }
    return result;
}

bool shouldEnableColor()
{
#ifdef __EMSCRIPTEN__
    return true;
#else
    return ISATTY(FILENO(stdout)) != 0;
#endif
}
} // namespace

Certificate::Certificate()
    : _use_color(shouldEnableColor())
{
}

TestStatus Certificate::getOverallStatus() const noexcept
{
    if (isFailed())
        return TestStatus::FAIL;

    for (const auto& result : _results)
        if (result.status == TestStatus::WARNING)
            return TestStatus::WARNING;

    for (const auto& nested : _nested_models)
    {
        if (nested.status == TestStatus::FAIL)
            return TestStatus::FAIL;
        if (nested.status == TestStatus::WARNING)
            return TestStatus::WARNING;
    }

    return TestStatus::PASS;
}

void Certificate::log(const std::string& message)
{
    if (!_quiet)
    {
        std::cout << (isColorEnabled() ? message : stripAnsi(message));
        std::cout << "\n";
    }

    _report_buffer.append(message);
    _report_buffer.append("\n");
}

void Certificate::printMainHeader(const std::string& hash)
{
    auto timestamp = std::chrono::system_clock::now();
    const std::time_t now_c = std::chrono::system_clock::to_time_t(timestamp);
    constexpr size_t TIME_STRING_BUFFER_SIZE = 100;
    std::array<char, TIME_STRING_BUFFER_SIZE> time_str{};

    std::tm time_info{};
#ifdef _WIN32
    // Check return value - gmtime_s returns errno_t (0 on success)
    if (gmtime_s(&time_info, &now_c) != 0)
    {
        log("╔════════════════════════════════════════════════════════════╗");
        log("║ MODEL VALIDATION REPORT                                    ║");
        log("╚════════════════════════════════════════════════════════════╝");
        log(std::format("Tool:       FSLint {}", PROJECT_VERSION));
        log("Timestamp:  [Error formatting timestamp]");
        log(std::format("SHA256:     {}", hash));
        return;
    }
#else
    // gmtime_r returns nullptr on failure
    if (gmtime_r(&now_c, &time_info) == nullptr)
    {
        log("╔════════════════════════════════════════════════════════════╗");
        log("║ MODEL VALIDATION REPORT                                    ║");
        log("╚════════════════════════════════════════════════════════════╝");
        log(std::format("Tool:       FSLint {}", PROJECT_VERSION));
        log("Timestamp:  [Error formatting timestamp]");
        log(std::format("SHA256:     {}", hash));
        return;
    }
#endif

    // strftime returns 0 on failure
    if (std::strftime(time_str.data(), time_str.size(), "%Y-%m-%d %H:%M:%S UTC", &time_info) == 0)
    {
        log("╔════════════════════════════════════════════════════════════╗");
        log("║ MODEL VALIDATION REPORT                                    ║");
        log("╚════════════════════════════════════════════════════════════╝");
        log(std::format("Tool:       FSLint {}", PROJECT_VERSION));
        log("Timestamp:  [Error formatting timestamp]");
        log(std::format("SHA256:     {}", hash));
        return;
    }

    log("╔════════════════════════════════════════════════════════════╗");
    log("║ MODEL VALIDATION REPORT                                    ║");
    log("╚════════════════════════════════════════════════════════════╝");
    log(std::format("Tool:       FSLint {}", PROJECT_VERSION));
    log(std::format("Timestamp:  {}", time_str.data()));
    log(std::format("SHA256:     {}", hash));
}

void Certificate::printSubsectionHeader(const std::string& name)
{
    // Reset subsection counters
    _current_subsection_passed = 0;
    _current_subsection_failed = 0;

    log("");
    log("┌────────────────────────────────────────┐");
    std::stringstream ss;
    constexpr int32_t SUBSECTION_NAME_WIDTH = 39;
    ss << "│ " << std::left << std::setw(static_cast<int32_t>(SUBSECTION_NAME_WIDTH)) << name << "│";
    log(ss.str());
    log("└────────────────────────────────────────┘");
    log("");
}

void Certificate::printTestResult(const TestResult& test)
{
    _results.push_back(test);

    std::string status_tag;
    if (test.status == TestStatus::PASS)
    {
        status_tag = "  [✓ PASS] ";
        _current_subsection_passed++;
    }
    else if (test.status == TestStatus::FAIL)
    {
        status_tag = std::format("  [{}{}{}] ", RED, "✗ FAIL", RESET);
        _current_subsection_failed++;
        _total_failed++;
    }
    else
    {
        status_tag = std::format("  [{}{}{}] ", YELLOW, "⚠ WARN", RESET);
        _current_subsection_passed++; // Warnings count as passed
    }

    log(std::format("{}{}", status_tag, test.test_name));

    if (test.status != TestStatus::PASS)
    {
        for (size_t i = 0; i < test.messages.size(); ++i)
        {
            const bool is_last = (i == test.messages.size() - 1);
            const std::string marker = is_last ? "└─ " : "├─ ";
            log(std::format("      {}{}", marker, test.messages[i]));
        }
    }

    // Handle security-related failures
    if (test.is_security_issue && test.status == TestStatus::FAIL && !_abort_requested)
    {
        if (_continue_callback)
        {
            if (!_continue_callback(test))
            {
                log("\n  [Aborting due to security issue]");
                _abort_requested = true;
            }
            else
            {
                log("\n  [Continuing after security issue as requested by user]");
            }
        }
        else
        {
            log("\n  [Aborting due to security issue (no continue callback set)]");
            _abort_requested = true;
        }
    }
}

void Certificate::printSubsectionSummary(bool subsection_valid)
{
    const bool actual_valid = subsection_valid && (_current_subsection_failed == 0);

    if (!actual_valid && _current_subsection_failed == 0)
        _total_failed++;

    log("");
    log("  ────────────────────────────────────────");
    log("  Tests: " + std::to_string(_current_subsection_passed) + " Passed, " +
        std::to_string(_current_subsection_failed) + " Failed");
    log("  Result: " + std::string(actual_valid ? "PASSED" : "FAILED"));
    log("  ────────────────────────────────────────");
}

void Certificate::addNestedModelResult(const NestedModelResult& result)
{
    _nested_models.push_back(result);
    if (result.status == TestStatus::FAIL)
        _total_failed++;
}

static void printTree(Certificate& cert, const std::vector<NestedModelResult>& models, const std::string& tree_prefix,
                      bool is_top_level)
{
    for (size_t i = 0; i < models.size(); ++i)
    {
        const bool is_last = (i == models.size() - 1);
        const auto& model = models[i];

        std::string status_tag;
        switch (model.status)
        {
        case TestStatus::PASS:
            status_tag = "[✓ PASS]";
            break;
        case TestStatus::WARNING:
            status_tag = std::string("[") + YELLOW + "⚠ WARN" + RESET + "]";
            break;
        case TestStatus::FAIL:
            status_tag = std::string("[") + RED + "✗ FAIL" + RESET + "]";
            break;
        }

        if (is_top_level)
        {
            cert.log(std::format("  {} {}", status_tag, model.name));
        }
        else
        {
            const std::string marker = is_last ? "└─ " : "├─ ";
            cert.log(std::format("  {} {}{}{}", status_tag, tree_prefix, marker, model.name));
        }

        if (!model.nested_models.empty())
        {
            std::string next_prefix = tree_prefix;
            if (!is_top_level)
                next_prefix += (is_last ? "   " : "│  ");
            printTree(cert, model.nested_models, next_prefix, false);
        }
    }
}

void Certificate::printNestedModelsTree()
{
    if (_nested_models.empty())
        return;

    printSubsectionHeader("NESTED MODELS");
    printTree(*this, _nested_models, "", true);
}

static void printFileTreeRecursive(Certificate& cert, const std::filesystem::path& path, const std::string& prefix)
{
    std::vector<std::filesystem::directory_entry> entries;
    for (const auto& entry : std::filesystem::directory_iterator(path))
        entries.push_back(entry);

    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b)
              {
                  if (a.is_directory() != b.is_directory())
                      return a.is_directory();
                  return file_utils::pathToUtf8(a.path().filename()) < file_utils::pathToUtf8(b.path().filename());
              });

    for (size_t i = 0; i < entries.size(); ++i)
    {
        const bool is_last = (i == entries.size() - 1);
        const auto& entry = entries[i];
        std::string name = file_utils::pathToUtf8(entry.path().filename());
        if (entry.is_regular_file() && file_utils::isBinary(entry.path()))
            name += " (binary)";

        const std::string marker = is_last ? "└── " : "├── ";

        cert.log(std::format("  {}{}{}", prefix, marker, name));

        if (entry.is_directory())
        {
            const std::string next_prefix = prefix + (is_last ? "    " : "│   ");
            printFileTreeRecursive(cert, entry.path(), next_prefix);
        }
    }
}

void Certificate::printFileTree(const std::filesystem::path& root)
{
    printSubsectionHeader("FILE TREE");
    log("  .");
    try
    {
        printFileTreeRecursive(*this, root, "");
    }
    catch (const std::exception& e)
    {
        log("  Error printing tree: " + std::string(e.what()));
    }
}

void Certificate::printFooter()
{
    log("");
    log("╔════════════════════════════════════════════════════════════╗");
    if (_total_failed == 0)
    {
        log("║  MODEL VALIDATION PASSED                                   ║");
    }
    else
    {
        std::stringstream ss;
        ss << "║  " << RED << "MODEL VALIDATION FAILED" << RESET << "                                   ║";
        log(ss.str());
    }
    log("╚════════════════════════════════════════════════════════════╝");
}

bool Certificate::saveToFile(const std::filesystem::path& path) const
{
    std::ofstream file(path);
    if (!file)
        return false;
    file << stripAnsi(_report_buffer);
    return true;
}

static void serializeNestedResults(const std::vector<NestedModelResult>& results, rapidjson::Value& array,
                                   rapidjson::Document::AllocatorType& allocator)
{
    for (const auto& res : results)
    {
        rapidjson::Value obj(rapidjson::kObjectType);
        obj.AddMember("name", rapidjson::Value(res.name.c_str(), allocator).Move(), allocator);

        std::string status;
        switch (res.status)
        {
        case TestStatus::PASS:
            status = "PASS";
            break;
        case TestStatus::FAIL:
            status = "FAIL";
            break;
        case TestStatus::WARNING:
            status = "WARNING";
            break;
        default:
            status = "UNKNOWN";
            break;
        }
        obj.AddMember("status", rapidjson::Value(status.c_str(), allocator).Move(), allocator);

        if (!res.nested_models.empty())
        {
            rapidjson::Value nested_array(rapidjson::kArrayType);
            serializeNestedResults(res.nested_models, nested_array, allocator);
            obj.AddMember("nested_models", nested_array, allocator);
        }

        array.PushBack(obj, allocator);
    }
}

std::string Certificate::toJson(const std::filesystem::path& root_path) const
{
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();

    // 1. Report (Keep ANSI codes for frontend highlighting)
    doc.AddMember("report", rapidjson::Value(_report_buffer.c_str(), allocator).Move(), allocator);

    // Overall Status
    std::string overall_status;
    switch (getOverallStatus())
    {
    case TestStatus::PASS:
        overall_status = "PASS";
        break;
    case TestStatus::FAIL:
        overall_status = "FAIL";
        break;
    case TestStatus::WARNING:
        overall_status = "WARNING";
        break;
    default:
        overall_status = "UNKNOWN";
        break;
    }
    doc.AddMember("overallStatus", rapidjson::Value(overall_status.c_str(), allocator).Move(), allocator);

    // 2. Summary
    rapidjson::Value summary(rapidjson::kObjectType);
    summary.AddMember("standard", rapidjson::Value(_summary.standard.c_str(), allocator).Move(), allocator);
    summary.AddMember("modelName", rapidjson::Value(_summary.modelName.c_str(), allocator).Move(), allocator);
    summary.AddMember("fmiVersion", rapidjson::Value(_summary.fmiVersion.c_str(), allocator).Move(), allocator);
    summary.AddMember("modelVersion", rapidjson::Value(_summary.modelVersion.c_str(), allocator).Move(), allocator);
    summary.AddMember("guid", rapidjson::Value(_summary.guid.c_str(), allocator).Move(), allocator);
    summary.AddMember("generationTool", rapidjson::Value(_summary.generationTool.c_str(), allocator).Move(), allocator);
    summary.AddMember("generationDateAndTime",
                      rapidjson::Value(_summary.generationDateAndTime.c_str(), allocator).Move(), allocator);
    summary.AddMember("author", rapidjson::Value(_summary.author.c_str(), allocator).Move(), allocator);
    summary.AddMember("copyright", rapidjson::Value(_summary.copyright.c_str(), allocator).Move(), allocator);
    summary.AddMember("license", rapidjson::Value(_summary.license.c_str(), allocator).Move(), allocator);
    summary.AddMember("description", rapidjson::Value(_summary.description.c_str(), allocator).Move(), allocator);
    summary.AddMember("hasIcon", _summary.hasIcon, allocator);
    summary.AddMember("sourceLanguage", rapidjson::Value(_summary.sourceLanguage.c_str(), allocator).Move(), allocator);
    summary.AddMember("totalSize", _summary.totalSize, allocator);

    rapidjson::Value fmuTypes(rapidjson::kArrayType);
    for (const auto& t : _summary.fmuTypes)
        fmuTypes.PushBack(rapidjson::Value(t.c_str(), allocator).Move(), allocator);
    summary.AddMember("fmuTypes", fmuTypes, allocator);

    rapidjson::Value platforms(rapidjson::kArrayType);
    for (const auto& p : _summary.platforms)
        platforms.PushBack(rapidjson::Value(p.c_str(), allocator).Move(), allocator);
    summary.AddMember("platforms", platforms, allocator);

    rapidjson::Value interfaces(rapidjson::kArrayType);
    for (const auto& i : _summary.interfaces)
        interfaces.PushBack(rapidjson::Value(i.c_str(), allocator).Move(), allocator);
    summary.AddMember("interfaces", interfaces, allocator);

    rapidjson::Value layeredStandards(rapidjson::kArrayType);
    for (const auto& s : _summary.layeredStandards)
        layeredStandards.PushBack(rapidjson::Value(s.c_str(), allocator).Move(), allocator);
    summary.AddMember("layeredStandards", layeredStandards, allocator);

    doc.AddMember("summary", summary, allocator);

    // 3. Test Results
    rapidjson::Value results(rapidjson::kArrayType);
    for (const auto& res : _results)
    {
        rapidjson::Value obj(rapidjson::kObjectType);
        obj.AddMember("test_name", rapidjson::Value(res.test_name.c_str(), allocator).Move(), allocator);

        std::string status;
        switch (res.status)
        {
        case TestStatus::PASS:
            status = "PASS";
            break;
        case TestStatus::FAIL:
            status = "FAIL";
            break;
        case TestStatus::WARNING:
            status = "WARNING";
            break;
        default:
            status = "UNKNOWN";
            break;
        }
        obj.AddMember("status", rapidjson::Value(status.c_str(), allocator).Move(), allocator);

        rapidjson::Value messages(rapidjson::kArrayType);
        for (const auto& msg : res.messages)
            messages.PushBack(rapidjson::Value(msg.c_str(), allocator).Move(), allocator);
        obj.AddMember("messages", messages, allocator);

        results.PushBack(obj, allocator);
    }
    doc.AddMember("results", results, allocator);

    // 4. Nested Models
    rapidjson::Value nested_models(rapidjson::kArrayType);
    serializeNestedResults(_nested_models, nested_models, allocator);
    doc.AddMember("nested_models", nested_models, allocator);

    // 5. File Tree (Optional)
    const std::filesystem::path& actual_root =
        (!root_path.empty() && std::filesystem::is_directory(root_path)) ? root_path : _extraction_path;

    if (!actual_root.empty() && std::filesystem::exists(actual_root) && std::filesystem::is_directory(actual_root))
    {
        rapidjson::Value tree;
        file_utils::fileNodeToJson(actual_root, &tree, &allocator);

        // Rename the root node to the model name (if available) or filename
        if (tree.IsObject() && tree.HasMember("name"))
        {
            std::string label = _summary.modelName;
            if (label.empty() && !root_path.empty())
                label = file_utils::pathToUtf8(root_path.filename());

            if (!label.empty())
                tree["name"].SetString(label.c_str(), static_cast<rapidjson::SizeType>(label.length()), allocator);
        }

        doc.AddMember("file_tree", tree, allocator);
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    return buffer.GetString();
}
