#pragma once

#include "checker.h"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

enum class TestStatus
{
    PASS,
    FAIL,
    WARNING
};

struct TestResult
{
    std::string test_name;
    TestStatus status;
    std::vector<std::string> messages; // Details about failures or warnings
};

class Certificate
{
  private:
    std::stringstream report_buffer;
    std::vector<TestResult> m_results;

    // Subsection tracking
    size_t current_subsection_passed = 0;
    size_t current_subsection_failed = 0;

  public:
    void log(const std::string& message);
    void printMainHeader(const std::string& filename, const std::string& hash);
    void printSubsectionHeader(const std::string& name);
    void printTestResult(const TestResult& test);
    void printSubsectionSummary(bool subsection_valid);
    void printFooter();

    // File Operations
    bool saveToFile(const std::filesystem::path& path) const;
    std::string getFullReport() const
    {
        return report_buffer.str();
    }

    const std::vector<TestResult>& getResults() const
    {
        return m_results;
    }
};
