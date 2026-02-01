#pragma once

#include <cctype>
#include <string>
#include <string_view>

/**
 * Parser for FMI structured naming convention.
 * Implements the BNF grammar for validating variable names.
 */
class StructuredNameParser
{
  public:
    /**
     * Validate a name according to the structured naming convention.
     * @param name The name to validate
     * @return true if the name is valid, false otherwise
     */
    static bool isValid(std::string_view name)
    {
        if (name.empty())
            return false;

        StructuredNameParser parser(name);
        return parser.parseName() && parser.isAtEnd();
    }

  private:
    explicit StructuredNameParser(std::string_view input)
        : input_(input)
        , pos_(0)
    {
    }

    // Character access helpers
    char current() const
    {
        return pos_ < input_.size() ? input_[pos_] : '\0';
    }

    char peek(size_t offset = 1) const
    {
        size_t peek_pos = pos_ + offset;
        return peek_pos < input_.size() ? input_[peek_pos] : '\0';
    }

    void advance()
    {
        if (pos_ < input_.size())
            ++pos_;
    }

    bool isAtEnd() const
    {
        return pos_ >= input_.size();
    }

    // BNF Grammar Rules

    // name = identifier | "der(" identifier ["," unsignedInteger ] ")"
    bool parseName()
    {
        // Check for "der(" prefix
        if (current() == 'd' && peek() == 'e' && peek(2) == 'r' && peek(3) == '(')
        {
            pos_ += 4; // Skip "der("

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

        char next = current();
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

    std::string_view input_;
    size_t pos_;
};
