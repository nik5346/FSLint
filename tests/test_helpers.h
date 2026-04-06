#pragma once

#include "certificate.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

inline bool has_fail(const Certificate& cert)
{
    const auto& results = cert.getResults();
    return std::any_of(results.begin(), results.end(),
                       [](const TestResult& r) { return r.getStatus() == TestStatus::FAIL; });
}

inline bool has_warning(const Certificate& cert)
{
    const auto& results = cert.getResults();
    return std::any_of(results.begin(), results.end(),
                       [](const TestResult& r) { return r.getStatus() == TestStatus::WARNING; });
}

inline bool has_error_with_text(const Certificate& cert, const std::string& text)
{
    const auto& results = cert.getResults();
    for (const auto& r : results)
    {
        if (r.getStatus() != TestStatus::FAIL)
            continue;
        if (r.getName().find(text) != std::string::npos)
            return true;
        for (const auto& msg : r.getMessages())
            if (msg.find(text) != std::string::npos)
                return true;
    }
    return false;
}

inline bool has_warning_with_text(const Certificate& cert, const std::string& text)
{
    const auto& results = cert.getResults();
    for (const auto& r : results)
    {
        if (r.getStatus() != TestStatus::WARNING && r.getStatus() != TestStatus::FAIL)
            continue;
        if (r.getName().find(text) != std::string::npos)
            return true;
        for (const auto& msg : r.getMessages())
            if (msg.find(text) != std::string::npos)
                return true;
    }
    return false;
}
