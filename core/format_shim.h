#pragma once

#ifdef USE_FMT_LIBRARY
#include <fmt/format.h>
namespace std {
    using fmt::format;
    using fmt::format_error;
    using fmt::runtime;
}
#else
#include <format>
#endif
