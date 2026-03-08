#pragma once

#include <filesystem>

#ifdef __EMSCRIPTEN__
#include <fmt/format.h>
namespace std
{
using fmt::format;
using fmt::format_error;
} // namespace std
#endif

class Certificate;

class Checker
{
  public:
    Checker() = default;
    virtual ~Checker() = default;

    // Disable copying and moving for interface class
    Checker(const Checker&) = delete;
    Checker& operator=(const Checker&) = delete;
    Checker(Checker&&) = delete;
    Checker& operator=(Checker&&) = delete;

    virtual void validate(const std::filesystem::path& path, Certificate& cert) = 0;
};
