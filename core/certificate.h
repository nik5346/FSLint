#pragma once

#include "checker.h"
#include "file_utils.h"

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
  public:
    Certificate();

  private:
    bool _quiet = false;
    bool _use_color = true;
    std::string _report_buffer;
    std::vector<TestResult> _results;
    std::vector<NestedModelResult> _nested_models;
    file_utils::FileNode _file_tree;

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

    void setUseColor(bool use)
    {
        _use_color = use;
    }
    bool isColorEnabled() const
    {
        return _use_color;
    }

    bool isFailed() const
    {
        return _total_failed > 0;
    }
    TestStatus getOverallStatus() const;
    void log(const std::string& message);
    void printMainHeader(const std::string& model_name, const std::string& hash);
    void printSubsectionHeader(const std::string& name);
    void printTestResult(const TestResult& test);
    void printSubsectionSummary(bool subsection_valid);
    void printFooter();

    void setFileTree(const file_utils::FileNode& tree)
    {
        _file_tree = tree;
    }
    const file_utils::FileNode& getFileTree() const
    {
        return _file_tree;
    }
    void printFileTree(const std::filesystem::path& root, const std::string& label);

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

    std::string toJson() const;
};
