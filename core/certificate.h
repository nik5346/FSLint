#pragma once

#include "checker.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
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
class TestResult;

/// @brief Callback function type for user-decided continuation on security issues.
/// @return True if validation should continue, false otherwise.
using ContinueCallback = std::function<bool(const TestResult&)>;

/// @brief Result of a single validation test.
class TestResult
{
  public:
    /// @brief Default constructor.
    TestResult() = default;

    /// @brief Constructor with automatic security issue detection.
    /// @param name Name of the test.
    /// @param s Completion status.
    /// @param msgs Detailed messages.
    TestResult(std::string name, TestStatus s, std::vector<std::string> msgs)
        : _test_name(std::move(name))
        , _status(s)
        , _messages(std::move(msgs))
        , _is_security_issue(_test_name.find("[SECURITY]") != std::string::npos)
    {
    }

    [[nodiscard]] const std::string& getName() const noexcept
    {
        return _test_name;
    }
    void setName(std::string name)
    {
        _test_name = std::move(name);
        _is_security_issue = _test_name.find("[SECURITY]") != std::string::npos;
    }

    [[nodiscard]] TestStatus getStatus() const noexcept
    {
        return _status;
    }
    void setStatus(TestStatus status) noexcept
    {
        _status = status;
    }

    [[nodiscard]] const std::vector<std::string>& getMessages() const noexcept
    {
        return _messages;
    }
    [[nodiscard]] std::vector<std::string>& getMessages() noexcept
    {
        return _messages;
    }

    [[nodiscard]] bool isSecurityIssue() const noexcept
    {
        return _is_security_issue;
    }

  private:
    std::string _test_name{};             ///< Name of the test.
    TestStatus _status{TestStatus::PASS}; ///< Completion status.
    std::vector<std::string> _messages{}; ///< Detailed failure or warning messages.
    bool _is_security_issue{false};       ///< True if this is a security-related test.
};

/// @brief Result of validation for a nested model (e.g., within resources).
struct NestedModelResult
{
    std::string name;                             ///< Path or name of the nested model.
    TestStatus status = TestStatus::PASS;         ///< Aggregated status.
    std::vector<NestedModelResult> nested_models; ///< Recursively nested results.
};

/// @brief Summary of extracted model metadata.
struct ModelSummary
{
    std::string standard{};                      ///< "FMI" or "SSP".
    std::string modelName{};                     ///< Name of the model.
    std::string fmiVersion{};                    ///< FMI version (e.g., "2.0").
    std::string modelVersion{};                  ///< Model version.
    std::string guid{};                          ///< GUID or instantiationToken.
    std::string generationTool{};                ///< Generating tool.
    std::string generationDateAndTime{};         ///< Generation timestamp.
    std::string author{};                        ///< Author.
    std::string copyright{};                     ///< Copyright.
    std::string license{};                       ///< License.
    std::string description{};                   ///< Model description.
    std::vector<std::string> platforms{};        ///< Supported platforms.
    std::vector<std::string> interfaces{};       ///< Supported interfaces.
    std::vector<std::string> layeredStandards{}; ///< Supported layered standards.
    bool hasIcon = false;                        ///< True if icon present.
    std::vector<std::string> fmuTypes{};         ///< FMU types ("Binary", "Source code").
    std::string sourceLanguage{};                ///< Programming language of sources.
    uint64_t totalSize = 0;                      ///< Total recursive size in bytes.
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

    // Abort handling
    ContinueCallback _continue_callback;
    bool _abort_requested = false;

  public:
    /// @brief Sets quiet mode.
    /// @param quiet Suppress detailed logging.
    void setQuiet(bool quiet) noexcept
    {
        _quiet = quiet;
    }

    /// @brief Sets the callback for continuing after security issues.
    /// @param callback Callback function.
    void setContinueCallback(ContinueCallback callback) noexcept
    {
        _continue_callback = std::move(callback);
    }

    /// @brief Checks if validation should abort.
    /// @return True if abort requested.
    [[nodiscard]] bool shouldAbort() const noexcept
    {
        return _abort_requested;
    }

    /// @brief Checks if color is enabled.
    /// @return True if color enabled.
    [[nodiscard]] bool isColorEnabled() const noexcept
    {
        return _use_color;
    }

    /// @brief Checks if any test failed.
    /// @return True if any failure was recorded.
    [[nodiscard]] bool isFailed() const noexcept
    {
        return _total_failed > 0;
    }

    /// @brief Gets overall status.
    /// @return Overall status.
    [[nodiscard]] TestStatus getOverallStatus() const noexcept;

    /// @brief Sets the extraction path.
    /// @param path Extraction directory path.
    void setExtractionPath(const std::filesystem::path& path)
    {
        _extraction_path = path;
    }

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
    [[nodiscard]] bool saveToFile(const std::filesystem::path& path) const;

    /// @brief Gets the full report text.
    /// @return Report string.
    [[nodiscard]] const std::string& getFullReport() const noexcept
    {
        return _report_buffer;
    }

    /// @brief Gets all test results.
    /// @return Vector of TestResult.
    [[nodiscard]] const std::vector<TestResult>& getResults() const noexcept
    {
        return _results;
    }

    /// @brief Gets all nested model results.
    /// @return Vector of NestedModelResult.
    [[nodiscard]] const std::vector<NestedModelResult>& getNestedModels() const noexcept
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
    [[nodiscard]] const ModelSummary& getSummary() const noexcept
    {
        return _summary;
    }

    /// @brief Converts report data to JSON.
    /// @param root_path Optional path to normalize results against.
    /// @return JSON string.
    [[nodiscard]] std::string toJson(const std::filesystem::path& root_path = "") const;

  private:
    std::filesystem::path _extraction_path;
};
