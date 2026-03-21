// iso8601.h — Header-only ISO 8601 date-time parser (C++17)
//
// Supports all ISO 8601 date-time representations:
//
//   Calendar (extended): YYYY  YYYY-MM  YYYY-MM-DD
//   Calendar (basic):    YYYYMMDD
//   Week (extended):     YYYY-Www  YYYY-Www-D
//   Week (basic):        YYYYWww   YYYYWwwD
//   Ordinal (extended):  YYYY-DDD
//   Ordinal (basic):     YYYYDDD
//   Date-times:          <date>T<time>[<tz>]
//   Time zone:           Z  |  +hh:mm  |  -hh:mm  |  (none = local)
//   Fractional seconds:  .fff or ,fff  (any number of digits)
//
// Usage:
//   auto r = iso8601::parse("2024-03-15T14:30:00Z");
//   if (r) {
//       r->year; r->month; r->day;
//       r->hour; r->minute; r->second; r->subSecond;
//       r->tz.utc; r->tz.offsetMinutes;
//   }
//
// Note: field values are not range-checked (e.g. month 13 will parse).
//       Validate ranges in your application if needed.

#pragma once

#include <charconv>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace iso8601
{

// ─── Result types ────────────────────────────────────────────────

struct Timezone
{
    bool utc = false;      // true  → Z
    bool local = false;    // true  → no tz suffix
    int offsetMinutes = 0; // e.g. +05:30 → +330, -08:00 → -480
};

struct DateTime
{
    int year = 0;
    // Calendar date (-1 = field absent)
    int month = -1; // 1–12
    int day = -1;   // 1–31
    // Week-based date (-1 = field absent)
    int week = -1;    // 1–53
    int weekday = -1; // 1 (Mon) – 7 (Sun)
    // Ordinal date (-1 = field absent)
    int yearDay = -1; // 1–366
    // Time (-1 = field absent)
    int hour = -1;
    int minute = -1;
    int second = -1;
    double subSecond = 0.0; // fractional seconds [0, 1)
    Timezone tz;
};

// ─── Internal parser ─────────────────────────────────────────────

namespace detail
{

// Non-owning cursor over the input string
struct Cursor
{
    const char* p; // current position
    const char* e; // one past end

    [[nodiscard]] bool atEnd() const noexcept
    {
        return p >= e;
    }

    [[nodiscard]] char peek(int n = 0) const noexcept
    {
        return (p + n < e) ? p[n] : '\0';
    }

    char get() noexcept
    {
        return *p++;
    }

    bool eat(char c) noexcept
    {
        if (p < e && *p == c)
        {
            ++p;
            return true;
        }
        return false;
    }

    [[nodiscard]] int remaining() const noexcept
    {
        return static_cast<int>(e - p);
    }
};

[[nodiscard]] inline bool isDigit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

// Consume exactly n decimal digits → out.  Does NOT advance on failure.
[[nodiscard]] inline bool takeDigits(Cursor& c, int n, int& out) noexcept
{
    if (c.remaining() < n)
        return false;
    for (int i = 0; i < n; ++i)
        if (!isDigit(c.p[i]))
            return false;
    out = 0;
    for (int i = 0; i < n; ++i)
        out = out * 10 + (c.p[i] - '0');
    c.p += n;
    return true;
}

// Count consecutive digits from current position (no advance)
[[nodiscard]] inline int countDigits(const Cursor& c) noexcept
{
    int n = 0;
    while (isDigit(c.peek(n)))
        ++n;
    return n;
}

// ── Timezone: Z | +hh[:mm] | -hh[:mm] | (empty) ─────────────────
[[nodiscard]] inline bool parseTimezone(Cursor& c, Timezone& tz) noexcept
{
    if (c.atEnd())
    {
        tz.local = true;
        return true;
    }
    if (c.eat('Z'))
    {
        tz.utc = true;
        return true;
    }
    if (c.peek() == '+' || c.peek() == '-')
    {
        const int sign = (c.get() == '+') ? 1 : -1;
        int hh = 0;
        int mm = 0;
        if (!takeDigits(c, 2, hh))
            return false;
        c.eat(':');
        if (!c.atEnd() && isDigit(c.peek()))
        {
            if (!takeDigits(c, 2, mm))
                return false;
        }
        tz.offsetMinutes = sign * (hh * 60 + mm);
        return true;
    }
    return false; // unexpected character
}

// ── Fractional seconds after the decimal point ───────────────────
inline void parseFractionalSeconds(Cursor& c, DateTime& dt) noexcept
{
    const char* fs = c.p;
    while (!c.atEnd() && isDigit(c.peek()))
        ++c.p;
    const int len = static_cast<int>(c.p - fs);
    if (len > 0)
    {
        double v = 0;
#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
        char* endptr = nullptr;
        const std::string s_str(fs, static_cast<size_t>(len));
        v = std::strtod(s_str.c_str(), &endptr);
#else
        std::from_chars(fs, c.p, v);
#endif
        dt.subSecond = v / std::pow(10.0, static_cast<double>(len));
    }
}

// ── Time: HH  HH:MM  HH:MM:SS  HH:MM:SS.fff
//          HH  HHMM   HHMMSS    HHMMSS.fff    ──────────────────────
[[nodiscard]] inline bool parseTime(Cursor& c, DateTime& dt) noexcept
{
    int hh = 0;
    if (!takeDigits(c, 2, hh))
        return false;
    dt.hour = hh;

    if (c.peek() == ':')
    {
        // Extended format
        c.get(); // ':'
        int mm = 0;
        if (!takeDigits(c, 2, mm))
            return false;
        dt.minute = mm;
        if (c.peek() == ':')
        {
            c.get(); // ':'
            int ss = 0;
            if (!takeDigits(c, 2, ss))
                return false;
            dt.second = ss;
            if (c.peek() == '.' || c.peek() == ',')
            {
                c.get();
                parseFractionalSeconds(c, dt);
            }
        }
    }
    else if (isDigit(c.peek()))
    {
        // Basic format
        int mm = 0;
        if (!takeDigits(c, 2, mm))
            return false;
        dt.minute = mm;
        if (isDigit(c.peek()))
        {
            int ss = 0;
            if (!takeDigits(c, 2, ss))
                return false;
            dt.second = ss;
            if (c.peek() == '.' || c.peek() == ',')
            {
                c.get();
                parseFractionalSeconds(c, dt);
            }
        }
    }
    return true;
}

// ── Date ─────────────────────────────────────────────────────────
//
// Strategy: peek ahead WITHOUT consuming to determine which variant
// we have, then consume exactly that many characters.
//
// After YYYY we may see:
//   (nothing / T / tz-char)   → year only
//   '-W'                      → extended week
//   'W'                       → basic week
//   '-' then 3 digits         → extended ordinal YYYY-DDD
//   '-' then 2 digits + '-'   → extended calendar YYYY-MM-DD
//   '-' then 2 digits + end   → extended year-month YYYY-MM
//   4+ digits                 → basic YYYYMMDD (8) or YYYYDDD (7) or YYYYMM (6)

[[nodiscard]] inline bool parseDate(Cursor& c, DateTime& dt) noexcept
{
    int yr = 0;
    if (!takeDigits(c, 4, yr))
        return false;
    dt.year = yr;

    const char nx = c.peek();

    // Year only: end of string, or T/t starts time, or tz character
    if (c.atEnd() || nx == 'T' || nx == 't' || nx == 'Z' || nx == '+')
        return true;
    // '-' could be: date separator OR negative tz offset (only tz if no digits follow)
    // A date '-' is always followed by a digit or 'W'
    if (nx == '-' && !isDigit(c.peek(1)) && c.peek(1) != 'W')
        return true; // negative tz, not ours

    // ── Extended format: starts with '-' ─────────────────────────
    if (nx == '-')
    {
        c.get(); // consume '-'

        // Week date: -Www[-D]
        if (c.peek() == 'W')
        {
            c.get();
            int wk = 0;
            if (!takeDigits(c, 2, wk))
                return false;
            dt.week = wk;
            if (c.eat('-'))
            {
                int wd = 0;
                if (!takeDigits(c, 1, wd))
                    return false;
                dt.weekday = wd;
            }
            return true;
        }

        // Count how many digits follow
        const int nd = countDigits(c);
        if (nd == 3)
        {
            // Ordinal YYYY-DDD
            int yd = 0;
            if (!takeDigits(c, 3, yd))
                return false;
            dt.yearDay = yd;
            return true;
        }
        if (nd >= 2)
        {
            // Month (and maybe day)
            int mm = 0;
            if (!takeDigits(c, 2, mm))
                return false;
            dt.month = mm;
            if (c.eat('-'))
            {
                int dd = 0;
                if (!takeDigits(c, 2, dd))
                    return false;
                dt.day = dd;
            }
            return true;
        }
        return false;
    }

    // ── Basic format: digits directly after year ──────────────────
    if (nx == 'W')
    {
        c.get();
        int wk = 0;
        if (!takeDigits(c, 2, wk))
            return false;
        dt.week = wk;
        if (isDigit(c.peek()))
        {
            int wd = 0;
            if (!takeDigits(c, 1, wd))
                return false;
            dt.weekday = wd;
        }
        return true;
    }

    // Must be digits
    const int nd = countDigits(c);
    if (nd == 3)
    {
        int yd = 0;
        if (!takeDigits(c, 3, yd))
            return false;
        dt.yearDay = yd;
        return true;
    }
    if (nd >= 4)
    {
        int mm = 0;
        int dd = 0;
        if (!takeDigits(c, 2, mm))
            return false;
        dt.month = mm;
        if (!takeDigits(c, 2, dd))
            return false;
        dt.day = dd;
        return true;
    }
    if (nd == 2)
    {
        int mm = 0;
        if (!takeDigits(c, 2, mm))
            return false;
        dt.month = mm;
        return true;
    }

    return false;
}

} // namespace detail

// ─── Public API ──────────────────────────────────────────────────

/// Parse an ISO 8601 date, time, or date-time string.
/// Returns std::nullopt on any error or trailing garbage.
[[nodiscard]] inline std::optional<DateTime> parse(std::string_view sv) noexcept
{
    if (sv.empty())
        return std::nullopt;
    detail::Cursor c{sv.data(), sv.data() + sv.size()};

    DateTime dt;

    // Detect time-only: starts with exactly 2 digits (HH), not 4+ (year).
    const int leading = detail::countDigits(c);
    if (leading == 2)
    {
        // Time-only string: HH, HH:MM, HH:MM:SS, etc.
        if (!detail::parseTime(c, dt))
            return std::nullopt;
    }
    else
    {
        if (!detail::parseDate(c, dt))
            return std::nullopt;
        if (c.eat('T') || c.eat('t'))
        {
            if (!detail::parseTime(c, dt))
                return std::nullopt;
        }
    }

    if (!detail::parseTimezone(c, dt.tz))
        return std::nullopt;
    if (!c.atEnd())
        return std::nullopt;
    return dt;
}

/// Returns true if the string is a valid ISO 8601 date/datetime/time.
[[nodiscard]] inline bool isValid(std::string_view sv) noexcept
{
    return parse(sv).has_value();
}

} // namespace iso8601
