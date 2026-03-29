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
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

/// @brief ISO 8601 parsing library.
namespace iso8601
{

// ─── Result types ────────────────────────────────────────────────

/// @brief Timezone information.
struct Timezone
{
    bool utc = false;      ///< True if Z.
    bool local = false;    ///< True if no suffix.
    int offsetMinutes = 0; ///< Offset in minutes.
};

/// @brief Parsed date and time components.
struct DateTime
{
    int year = 0;           ///< Year.
    int month = -1;         ///< 1-12.
    int day = -1;           ///< 1-31.
    int week = -1;          ///< 1-53.
    int weekday = -1;       ///< 1-7.
    int yearDay = -1;       ///< 1-366.
    int hour = -1;          ///< 0-23.
    int minute = -1;        ///< 0-59.
    int second = -1;        ///< 0-60.
    double subSecond = 0.0; ///< Fractional seconds.
    Timezone tz;            ///< Timezone.
};

// ─── Internal parser ─────────────────────────────────────────────

/// @brief Non-owning cursor over input string.
class Cursor
{
  public:
    /// @brief Constructor.
    /// @param sv Input string view.
    explicit Cursor(std::string_view sv) noexcept
        : _p(sv.data())
        , _e(sv.data() + sv.size())
    {
    }

    /// @brief Checks if at end.
    /// @return True if at end.
    [[nodiscard]] bool atEnd() const noexcept
    {
        return _p >= _e;
    }

    /// @brief Peeks at next character.
    /// @param n Offset.
    /// @return Character or null.
    [[nodiscard]] char peek(int n = 0) const noexcept
    {
        return (_p + n < _e) ? _p[n] : '\0';
    }

    /// @brief Gets next character.
    /// @return Character.
    char get() noexcept
    {
        return *_p++;
    }

    /// @brief Consumes character if it matches.
    /// @param c Character to match.
    /// @return True if matched.
    bool eat(char c) noexcept
    {
        if (_p < _e && *_p == c)
        {
            ++_p;
            return true;
        }
        return false;
    }

    /// @brief Gets remaining character count.
    /// @return Count.
    [[nodiscard]] int remaining() const noexcept
    {
        return static_cast<int>(_e - _p);
    }

    /// @brief Gets pointer to current data.
    /// @return Pointer.
    [[nodiscard]] const char* data() const noexcept
    {
        return _p;
    }

    /// @brief Skips characters.
    /// @param n Count.
    void skip(int n) noexcept
    {
        _p += n;
    }

  private:
    const char* _p; // current position
    const char* _e; // one past end
};

/// @brief Checks if digit.
/// @param c Character.
/// @return True if digit.
[[nodiscard]] inline bool isDigit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

/// @brief Consumes n digits.
/// @param c Cursor.
/// @param n Count.
/// @param out Output integer.
/// @return True if success.
[[nodiscard]] inline bool takeDigits(Cursor& c, int n, int& out) noexcept
{
    if (c.remaining() < n)
        return false;
    for (int i = 0; i < n; ++i)
        if (!isDigit(c.peek(i)))
            return false;
    out = 0;
    for (int i = 0; i < n; ++i)
        out = out * 10 + (c.peek(i) - '0');
    c.skip(n);
    return true;
}

/// @brief Counts consecutive digits.
/// @param c Cursor.
/// @return Count.
[[nodiscard]] inline int countDigits(const Cursor& c) noexcept
{
    int n = 0;
    while (isDigit(c.peek(n)))
        ++n;
    return n;
}

/// @brief Parses timezone.
/// @param c Cursor.
/// @param tz Output timezone.
/// @return True if success.
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

/// @brief Parses fractional seconds.
/// @param c Cursor.
/// @param dt Output DateTime.
inline void parseFractionalSeconds(Cursor& c, DateTime& dt) noexcept
{
    const char* fs = c.data();
    while (!c.atEnd() && isDigit(c.peek()))
        c.skip(1);
    const int len = static_cast<int>(c.data() - fs);
    if (len > 0)
    {
        double v = 0;
#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
        char* endptr = nullptr;
        const std::string s_str(fs, static_cast<size_t>(len));
        v = std::strtod(s_str.c_str(), &endptr);
#else
        std::from_chars(fs, c.data(), v);
#endif
        dt.subSecond = v / std::pow(10.0, static_cast<double>(len));
    }
}

/// @brief Parses time component.
/// @param c Cursor.
/// @param dt Output DateTime.
/// @return True if success.
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

/// @brief Parses date component.
/// @param c Cursor.
/// @param dt Output DateTime.
/// @return True if success.
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

// ─── Public API ──────────────────────────────────────────────────

/// @brief Parse an ISO 8601 date, time, or date-time string.
/// @param sv Input string.
/// @return Optional DateTime.
[[nodiscard]] inline std::optional<DateTime> parse(std::string_view sv) noexcept
{
    if (sv.empty())
        return std::nullopt;
    Cursor c{sv};

    DateTime dt;

    // Detect time-only: starts with exactly 2 digits (HH), not 4+ (year).
    const int leading = countDigits(c);
    if (leading == 2)
    {
        // Time-only string: HH, HH:MM, HH:MM:SS, etc.
        if (!parseTime(c, dt))
            return std::nullopt;
    }
    else
    {
        if (!parseDate(c, dt))
            return std::nullopt;
        if (c.eat('T') || c.eat('t'))
        {
            if (!parseTime(c, dt))
                return std::nullopt;
        }
    }

    if (!parseTimezone(c, dt.tz))
        return std::nullopt;
    if (!c.atEnd())
        return std::nullopt;
    return dt;
}

/// @brief Returns true if the string is a valid ISO 8601 date/datetime/time.
/// @param sv Input string.
/// @return True if valid.
[[nodiscard]] inline bool isValid(std::string_view sv) noexcept
{
    return parse(sv).has_value();
}

} // namespace iso8601
