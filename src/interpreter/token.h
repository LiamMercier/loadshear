// Copyright (c) 2026 Liam Mercier
//
// This file is part of Loadshear.
//
// Loadshear is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License Version 3.0
// as published by the Free Software Foundation.
//
// Loadshear is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License v3.0
// for more details.
//
// You should have received a copy of the GNU General Public License v3.0
// along with Loadshear. If not, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#pragma once

#include <cstdint>
#include <unordered_set>

constexpr std::string_view VALID_TOKEN_OPERATORS = "=,:";

const std::unordered_set<std::string> VALID_KEYWORDS {
    "SETTINGS",
    "SESSION",
    "PORT",
    "HEADERSIZE",
    "BODYMAX",
    "READ",
    "REPEAT",
    "SAMPLERATE",
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
    "COUNTER"
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
