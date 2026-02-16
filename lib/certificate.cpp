#include "certificate.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

TestStatus Certificate::getOverallStatus() const
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
        std::cout << message;
        std::cout << "\n";
    }

    _report_buffer << message;
    _report_buffer << "\n";
}

void Certificate::printMainHeader(const std::string& filename, const std::string& hash)
{
    auto timestamp = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(timestamp);
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
        log("Model Path: " + filename);
        log("SHA256:     " + hash);
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
        log("Model Path: " + filename);
        log("SHA256:     " + hash);
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
        log("Model Path: " + filename);
        log("SHA256:     " + hash);
        return;
    }

    log("╔════════════════════════════════════════════════════════════╗");
    log("║ MODEL VALIDATION REPORT                                    ║");
    log("╚════════════════════════════════════════════════════════════╝");
    log(std::format("Tool:       FSLint {}", PROJECT_VERSION));
    log("Timestamp:  " + std::string(time_str.data()));
    log("Model Path: " + filename);
    log("SHA256:     " + hash);
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

    std::stringstream ss;
    if (test.status == TestStatus::PASS)
    {
        ss << "  [✓ PASS] ";
        _current_subsection_passed++;
    }
    else if (test.status == TestStatus::FAIL)
    {
        ss << "  [✗ FAIL] ";
        _current_subsection_failed++;
        _total_failed++;
    }
    else
    {
        ss << "  [⚠ WARN] ";
        _current_subsection_passed++; // Warnings count as passed
    }

    ss << test.test_name;
    log(ss.str());

    for (const auto& msg : test.messages)
        log("      └─ " + msg);
}

void Certificate::printSubsectionSummary(bool subsection_valid)
{
    bool actual_valid = subsection_valid && (_current_subsection_failed == 0);

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

static void printTree(Certificate& cert, const std::vector<NestedModelResult>& models, const std::string& prefix)
{
    for (size_t i = 0; i < models.size(); ++i)
    {
        bool is_last = (i == models.size() - 1);
        const auto& model = models[i];

        std::string marker = is_last ? "└─ " : "├─ ";
        std::string status_str;
        switch (model.status)
        {
        case TestStatus::PASS:
            status_str = "[✓ PASS]";
            break;
        case TestStatus::WARNING:
            status_str = "[⚠ WARN]";
            break;
        case TestStatus::FAIL:
            status_str = "[✗ FAIL]";
            break;
        }

        cert.log(prefix + marker + model.name + " " + status_str);

        if (!model.nested_models.empty())
        {
            std::string new_prefix = prefix + (is_last ? "   " : "│  ");
            printTree(cert, model.nested_models, new_prefix);
        }
    }
}

void Certificate::printNestedModelsTree()
{
    if (_nested_models.empty())
        return;

    printSubsectionHeader("NESTED MODELS");
    printTree(*this, _nested_models, "  ");
}

void Certificate::printFooter()
{
    log("");
    log("╔════════════════════════════════════════════════════════════╗");
    if (_total_failed == 0)
        log("║ ✓ MODEL VALIDATION PASSED                                  ║");
    else
        log("║ ✗ MODEL VALIDATION FAILED                                  ║");
    log("╚════════════════════════════════════════════════════════════╝");
}

bool Certificate::saveToFile(const std::filesystem::path& path) const
{
    std::ofstream file(path);
    if (!file)
        return false;
    file << _report_buffer.str();
    return true;
}
