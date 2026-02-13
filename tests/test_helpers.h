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
