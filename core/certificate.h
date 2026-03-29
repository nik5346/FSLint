#pragma once

#include "checker.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

/// @brief Possible statuses of a validation test.
enum class TestStatus : uint8_t
{
    PASS,   ///< All requirements met.
    FAIL,   ///< Standard violation detected.
    WARNING ///< Potential issue or recommendation.
};

/// @brief Result of a single validation test.
struct TestResult
{
    std::string test_name;             ///< Name of the test.
    TestStatus status;                 ///< Completion status.
    std::vector<std::string> messages; ///< Detailed failure or warning messages.
};

/// @brief Result of validation for a nested model (e.g., within resources).
struct NestedModelResult
{
    std::string name;                             ///< Path or name of the nested model.
    TestStatus status;                            ///< Aggregated status.
    std::vector<NestedModelResult> nested_models; ///< Recursively nested results.
};

/// @brief Summary of extracted model metadata.
struct ModelSummary
{
    std::string standard;                      ///< "FMI" or "SSP".
    std::string modelName;                     ///< Name of the model.
    std::string fmiVersion;                    ///< FMI version (e.g., "2.0").
    std::string modelVersion;                  ///< Model version.
    std::string guid;                          ///< GUID or instantiationToken.
    std::string generationTool;                ///< Generating tool.
    std::string generationDateAndTime;         ///< Generation timestamp.
    std::string author;                        ///< Author.
    std::string copyright;                     ///< Copyright.
    std::string license;                       ///< License.
    std::string description;                   ///< Model description.
    std::vector<std::string> platforms;        ///< Supported platforms.
    std::vector<std::string> interfaces;       ///< Supported interfaces.
    std::vector<std::string> layeredStandards; ///< Supported layered standards.
    bool hasIcon = false;                      ///< True if icon present.
    std::vector<std::string> fmuTypes;         ///< FMU types ("Binary", "Source code").
    std::string sourceLanguage;                ///< Programming language of sources.
    uint64_t totalSize = 0;                    ///< Total recursive size in bytes.
};

/// @brief Validation report generator and result container.
class Certificate
{
  public:
    /// @brief Default constructor.
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
    /// @brief Sets quiet mode.
    /// @param quiet Suppress detailed logging.
    void setQuiet(bool quiet)
    {
        _quiet = quiet;
    }

    /// @brief Checks if quiet mode is active.
    /// @return True if quiet.
    bool isQuiet() const
    {
        return _quiet;
    }

    /// @brief Sets use of color in console output.
    /// @param use Use ANSI color codes.
    void setUseColor(bool use)
    {
        _use_color = use;
    }

    /// @brief Checks if color is enabled.
    /// @return True if color enabled.
    bool isColorEnabled() const
    {
        return _use_color;
    }

    /// @brief Checks if any test failed.
    /// @return True if any failure was recorded.
    bool isFailed() const
    {
        return _total_failed > 0;
    }

    /// @brief Gets overall status.
    /// @return Overall status.
    TestStatus getOverallStatus() const;

    /// @brief Appends a message to the report buffer.
    /// @param message Message to log.
    void log(const std::string& message);

    /// @brief Prints report header.
    /// @param hash Model file hash.
    void printMainHeader(const std::string& hash);

    /// @brief Prints subsection header.
    /// @param name Subsection name.
    void printSubsectionHeader(const std::string& name);

    /// @brief Prints test result.
    /// @param test Result to print.
    void printTestResult(const TestResult& test);

    /// @brief Prints summary for current subsection.
    /// @param subsection_valid True if all tests in subsection passed.
    void printSubsectionSummary(bool subsection_valid);

    /// @brief Prints report footer.
    void printFooter();

    /// @brief Prints file tree of extracted model.
    /// @param root Extraction directory.
    void printFileTree(const std::filesystem::path& root);

    /// @brief Adds a result for a nested model.
    /// @param result Result to add.
    void addNestedModelResult(const NestedModelResult& result);

    /// @brief Prints status tree of nested models.
    void printNestedModelsTree();

    /// @brief Saves report to file.
    /// @param path Destination path.
    /// @return True if saved.
    bool saveToFile(const std::filesystem::path& path) const;

    /// @brief Gets the full report text.
    /// @return Report string.
    std::string getFullReport() const
    {
        return _report_buffer;
    }

    /// @brief Gets all test results.
    /// @return Vector of TestResult.
    const std::vector<TestResult>& getResults() const
    {
        return _results;
    }

    /// @brief Gets all nested model results.
    /// @return Vector of NestedModelResult.
    const std::vector<NestedModelResult>& getNestedModels() const
    {
        return _nested_models;
    }

    /// @brief Sets the model summary.
    /// @param summary Summary object.
    void setSummary(const ModelSummary& summary)
    {
        _summary = summary;
    }

    /// @brief Gets the model summary.
    /// @return Summary object.
    const ModelSummary& getSummary() const
    {
        return _summary;
    }

    /// @brief Converts report data to JSON.
    /// @param root_path Optional path to normalize results against.
    /// @return JSON string.
    std::string toJson(const std::filesystem::path& root_path = "") const;

    /// @brief Sets extraction path.
    /// @param path Directory where model was extracted.
    void setExtractionPath(const std::filesystem::path& path)
    {
        _extraction_path = path;
    }

  private:
    std::filesystem::path _extraction_path;
};
