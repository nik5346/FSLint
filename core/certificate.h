#pragma once

#include "checker.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

enum class TestStatus : uint8_t
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

struct NestedModelResult
{
    std::string name;
    TestStatus status;
    std::vector<NestedModelResult> nested_models;
};

class Certificate
{
  private:
    bool _quiet = false;
    std::string _report_buffer;
    std::vector<TestResult> _results;
    std::vector<NestedModelResult> _nested_models;

    // Subsection tracking
    size_t _current_subsection_passed = 0;
    size_t _current_subsection_failed = 0;
    size_t _total_failed = 0;

  public:
    void setQuiet(bool quiet)
    {
        _quiet = quiet;
    }
    bool isQuiet() const
    {
        return _quiet;
    }

    bool isFailed() const
    {
        return _total_failed > 0;
    }
    TestStatus getOverallStatus() const;
    void log(const std::string& message);
    void printMainHeader(const std::string& filename, const std::string& hash);
    void printSubsectionHeader(const std::string& name);
    void printTestResult(const TestResult& test);
    void printSubsectionSummary(bool subsection_valid);
    void printFooter();

    void addNestedModelResult(const NestedModelResult& result);
    void printNestedModelsTree();

    // File Operations
    bool saveToFile(const std::filesystem::path& path) const;
    std::string getFullReport() const
    {
        return _report_buffer;
    }

    const std::vector<TestResult>& getResults() const
    {
        return _results;
    }

    const std::vector<NestedModelResult>& getNestedModels() const
    {
        return _nested_models;
    }
};
