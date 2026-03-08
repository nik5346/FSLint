#pragma once

#ifdef USE_FMT_LIBRARY
#include <fmt/format.h>
namespace fslint
{
using fmt::format;
using fmt::format_error;
} // namespace fslint
#else
#include <format>
namespace fslint
{
using std::format;
using std::format_error;
} // namespace fslint
#endif
