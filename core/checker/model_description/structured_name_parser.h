#pragma once

#include <cstddef>
#include <string_view>

/// @brief Recursive descent parser for FMI structured variable names.
///
/// Validates variable names against the BNF grammar defined in the FMI specification.
class StructuredNameParser
{
  public:
    /// @brief Validates if a name follows the structured naming convention.
    /// @param name The name to validate.
    /// @return True if valid.
    static bool isValid(std::string_view name)
    {
        if (name.empty())
            return false;

        StructuredNameParser parser(name);
        return parser.parseName() && parser.isAtEnd();
    }

  private:
    explicit StructuredNameParser(std::string_view input)
        : _input(input)
    {
    }

    // Character access helpers
    char current() const
    {
        return _pos < _input.size() ? _input[_pos] : '\0';
    }

    char peek(size_t offset = 1) const
    {
        const size_t peek_pos = _pos + offset;
        return peek_pos < _input.size() ? _input[peek_pos] : '\0';
    }

    void advance()
    {
        if (_pos < _input.size())
            ++_pos;
    }

    bool isAtEnd() const
    {
        return _pos >= _input.size();
    }

    // BNF Grammar Rules

    // name = identifier | "der(" identifier ["," unsignedInteger ] ")"
    bool parseName()
    {
        // Check for "der(" prefix
        if (current() == 'd' && peek() == 'e' && peek(2) == 'r' && peek(3) == '(')
        {
            _pos += 4; // Skip "der("

            if (!parseIdentifier())
                return false;

            // Optional: "," unsignedInteger
            if (current() == ',')
            {
                advance(); // Skip ','

                if (!parseUnsignedInteger())
                    return false;
            }

            if (current() != ')')
                return false;

            advance(); // Skip ')'
            return true;
        }

        // Otherwise, must be identifier
        return parseIdentifier();
    }

    // identifier = B-name [ arrayIndices ] {"." B-name [ arrayIndices ] }
    bool parseIdentifier()
    {
        if (!parseBName())
            return false;

        // Optional arrayIndices after first B-name
        if (current() == '[')
        {
            if (!parseArrayIndices())
                return false;
        }

        // Zero or more: "." B-name [ arrayIndices ]
        while (current() == '.')
        {
            advance(); // Skip '.'

            if (!parseBName())
                return false;

            // Optional arrayIndices
            if (current() == '[')
            {
                if (!parseArrayIndices())
                    return false;
            }
        }

        return true;
    }

    // B-name = nondigit { digit | nondigit } | Q-name
    bool parseBName()
    {
        // Try Q-name first (quoted string)
        if (current() == '\'')
            return parseQName();

        // Otherwise: nondigit { digit | nondigit }
        if (!isNondigit(current()))
            return false;

        advance();

        // Zero or more digit or nondigit
        while (isDigit(current()) || isNondigit(current()))
            advance();

        return true;
    }

    // Q-name = "'" ( Q-char | escape ) { Q-char | escape } "'"
    bool parseQName()
    {
        if (current() != '\'')
            return false;

        advance(); // Skip opening quote

        // Must have at least one character (Q-char or escape)
        if (!parseQCharOrEscape())
            return false;

        // Zero or more Q-char or escape
        while (current() != '\'' && current() != '\0')
            if (!parseQCharOrEscape())
                return false;

        // Must end with closing quote
        if (current() != '\'')
            return false;

        advance(); // Skip closing quote
        return true;
    }

    // Parse either Q-char or escape sequence
    bool parseQCharOrEscape()
    {
        if (current() == '\\')
        {
            return parseEscape();
        }
        else if (isQChar(current()))
        {
            advance();
            return true;
        }

        return false;
    }

    // escape = "\'" | "\"" | "\?" | "\\" | "\a" | "\b" | "\f" | "\n" | "\r" | "\t" | "\v"
    bool parseEscape()
    {
        if (current() != '\\')
            return false;

        advance(); // Skip backslash

        const char next = current();
        if (next == '\'' || next == '"' || next == '?' || next == '\\' || next == 'a' || next == 'b' || next == 'f' ||
            next == 'n' || next == 'r' || next == 't' || next == 'v')
        {
            advance();
            return true;
        }

        return false;
    }

    // arrayIndices = "[" unsignedInteger {"," unsignedInteger} "]"
    bool parseArrayIndices()
    {
        if (current() != '[')
            return false;

        advance(); // Skip '['

        // Must have at least one unsignedInteger
        if (!parseUnsignedInteger())
            return false;

        // Zero or more: "," unsignedInteger
        while (current() == ',')
        {
            advance(); // Skip ','

            if (!parseUnsignedInteger())
                return false;
        }

        if (current() != ']')
            return false;

        advance(); // Skip ']'
        return true;
    }

    // unsignedInteger = digit { digit }
    bool parseUnsignedInteger()
    {
        if (!isDigit(current()))
            return false;

        advance();

        // Zero or more digits
        while (isDigit(current()))
            advance();

        return true;
    }

    // Character classification helpers

    // nondigit = "_" | letters "a" to "z" | letters "A" to "Z"
    bool isNondigit(char c) const
    {
        return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    // digit = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
    bool isDigit(char c) const
    {
        return c >= '0' && c <= '9';
    }

    // Q-char = nondigit | digit | "!" | "#" | "$" | "%" | "&" | "(" | ")" |
    //          "*" | "+" | "," | "-" | "." | "/" | ":" | ";" | "<" | ">" |
    //          "=" | "?" | "@" | "[" | "]" | "^" | "{" | "}" | "|" | "~" | " "
    bool isQChar(char c) const
    {
        if (isNondigit(c) || isDigit(c))
            return true;

        switch (c)
        {
        case '!':
        case '#':
        case '$':
        case '%':
        case '&':
        case '(':
        case ')':
        case '*':
        case '+':
        case ',':
        case '-':
        case '.':
        case '/':
        case ':':
        case ';':
        case '<':
        case '>':
        case '=':
        case '?':
        case '@':
        case '[':
        case ']':
        case '^':
        case '{':
        case '}':
        case '|':
        case '~':
        case ' ':
            return true;
        default:
            return false;
        }
    }

    std::string_view _input;
    size_t _pos = 0;
};
