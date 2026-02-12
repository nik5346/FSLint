#pragma once

#include "certificate.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

inline bool has_fail(const Certificate& cert)
{
    const auto& results = cert.getResults();
    return std::any_of(results.begin(), results.end(),
                       [](const TestResult& r) { return r.status == TestStatus::FAIL; });
}

inline bool has_warning(const Certificate& cert)
{
    const auto& results = cert.getResults();
    return std::any_of(results.begin(), results.end(),
                       [](const TestResult& r) { return r.status == TestStatus::WARNING; });
}

inline bool has_error_with_text(const Certificate& cert, const std::string& text)
{
    const auto& results = cert.getResults();
    for (const auto& r : results)
    {
        if (r.status != TestStatus::FAIL)
        {
            continue;
        }
        if (r.test_name.find(text) != std::string::npos)
        {
            return true;
        }
        for (const auto& msg : r.messages)
        {
            if (msg.find(text) != std::string::npos)
            {
                return true;
            }
        }
    }
    return false;
}

class TemporaryDirectory
{
public:
    TemporaryDirectory(const std::string& prefix)
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        path = fs::temp_directory_path() / (prefix + "_" + std::to_string(nanos));
        fs::create_directories(path);
    }

    ~TemporaryDirectory()
    {
        try {
            fs::remove_all(path);
        } catch (...) {}
    }

    void writeFile(const fs::path& rel_path, const std::string& content)
    {
        fs::path p = path / rel_path;
        fs::create_directories(p.parent_path());
        std::ofstream(p) << content;
    }

    void createDir(const fs::path& rel_path)
    {
        fs::create_directories(path / rel_path);
    }

    const fs::path& getPath() const
    {
        return path;
    }

    operator fs::path() const
    {
        return path;
    }

    operator std::string() const
    {
        return path.string();
    }

private:
    fs::path path;
};

inline bool has_warning_with_text(const Certificate& cert, const std::string& text)
{
    const auto& results = cert.getResults();
    for (const auto& r : results)
    {
        if (r.status != TestStatus::WARNING)
            continue;
        if (r.test_name.find(text) != std::string::npos)
            return true;
        for (const auto& msg : r.messages)
            if (msg.find(text) != std::string::npos)
                return true;
    }
    return false;
}
