#pragma once

#include <cstdint>
#include <unordered_set>

constexpr std::string_view VALID_TOKEN_OPERATORS = "=,:";

const std::unordered_set<std::string> VALID_KEYWORDS {
    "SETTINGS",
    "SESSION",
    "HEADERSIZE",
    "BODYMAX",
    "READ",
    "REPEAT",
    "ENDPOINTS",
    "SHARDS",
    "PACKETS",
    "HANDLER",
    "ORCHESTRATOR",
    "CREATE",
    "CONNECT",
    "SEND",
    "FLOOD",
    "DRAIN",
    "DISCONNECT",
    "OFFSET",
    "COPIES",
    "TIMESTAMP",
    "COUNTER",
    "FORMAT",
    "TIMEOUT"
};

enum class TokenType : uint8_t
{
    Keyword = 0,
    Identity,
    Operator,
    Value,
    BraceOpen,
    BraceClosed
};

// Utility to print type name.
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
