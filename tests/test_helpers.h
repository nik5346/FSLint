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
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);

    const auto& results = cert.getResults();
    for (const auto& r : results)
    {
        if (r.status != TestStatus::WARNING && r.status != TestStatus::FAIL)
            continue;

        std::string lower_test_name = r.test_name;
        std::transform(lower_test_name.begin(), lower_test_name.end(), lower_test_name.begin(), ::tolower);
        if (lower_test_name.find(lower_text) != std::string::npos)
            return true;

        for (const auto& msg : r.messages)
        {
            std::string lower_msg = msg;
            std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);
            if (lower_msg.find(lower_text) != std::string::npos)
                return true;
        }
    }
    return false;
}
