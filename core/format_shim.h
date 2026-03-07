#pragma once

#ifdef USE_FMT_LIBRARY
#include <fmt/format.h>
namespace fslint
{
using fmt::format;
using fmt::format_error;
using fmt::runtime;
} // namespace fslint
#else
#include <format>
namespace fslint
{
using std::format;
using std::format_error;

// std::runtime_format is C++26, but fmtlib has fmt::runtime.
// We provide a fallback for std::runtime_format if we're on a newer compiler.
#if __cpp_lib_format >= 202207L
using std::runtime_format;
#endif

} // namespace fslint
#endif
