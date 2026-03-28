#pragma once

#include "checker.h"

#include <cstddef>
#include <cstdint>
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

struct ModelSummary
{
    std::string standard; // "FMI" or "SSP"
    std::string modelName;
    std::string fmiVersion;
    std::string modelVersion;
    std::string guid;
    std::string generationTool;
    std::string generationDateAndTime;
    std::string author;
    std::string copyright;
    std::string license;
    std::string description;
    std::vector<std::string> platforms;
    std::vector<std::string> interfaces;
    std::vector<std::string> layeredStandards;
    bool hasIcon = false;
    std::vector<std::string> fmuTypes; // "Binary" and/or "Source code"
    std::string sourceLanguage;
    uint64_t totalSize = 0;
};

class Certificate
{
  public:
    Certificate();

  private:
    bool _quiet = true;
    bool _use_color = true;
    std::string _report_buffer;
    std::vector<TestResult> _results;
    std::vector<NestedModelResult> _nested_models;
    ModelSummary _summary;

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
    void printMainHeader(const std::string& hash);
    void printSubsectionHeader(const std::string& name);
    void printTestResult(const TestResult& test);
    void printSubsectionSummary(bool subsection_valid);
    void printFooter();

    void printFileTree(const std::filesystem::path& root);

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

    void setSummary(const ModelSummary& summary)
    {
        _summary = summary;
    }
    const ModelSummary& getSummary() const
    {
        return _summary;
    }

    std::string toJson(const std::filesystem::path& root_path = "") const;

    void setExtractionPath(const std::filesystem::path& path)
    {
        _extraction_path = path;
    }

  private:
    std::filesystem::path _extraction_path;
};
