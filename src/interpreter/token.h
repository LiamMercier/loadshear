#pragma once

enum class TokenType
{
    Keyword = 0,
    Identity,
    Operator,
    Value,
    BraceOpen,
    BraceClosed
};

// Debug utility to print type name.
inline std::string ttype_to_string(TokenType ttype)
{
    switch (ttype)
    {
        case TokenType::Keyword:
        {
            return "Keyword";
        }
        case TokenType::Identity:
        {
            return "Identity";
        }
        case TokenType::Operator:
        {
            return "Operator";
        }
        case TokenType::Value:
        {
            return "Value";
        }
        case TokenType::BraceOpen:
        {
            return "BraceOpen";
        }
        case TokenType::BraceClosed:
        {
            return "BraceClosed";
        }
        default:
        {
            return "";
        }
    }
}

struct Token
{
    TokenType type;
    std::string text;
    size_t line;
    size_t col;
};
