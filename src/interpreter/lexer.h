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

#include <string>
#include <vector>

#include "token.h"
#include "parse-result.h"
#include "diagnostic-colors.h"

class Lexer
{
public:
    Lexer(std::string source);

    // Tokenize the entire text and put it into the referenced vector.
    ParseResult tokenize(std::vector<Token> & tokens);

private:
    inline void jump_next_line();

    inline char peek() const;

    inline char peek_next() const;

    char advance();

    inline bool eof() const;

    inline void skip_ignorable();

    inline Token make_token(TokenType type,
                            std::string text,
                            size_t line,
                            size_t column);

private:
    std::string script_source_;
    size_t pos_ = 0;
    size_t line_ = 1;
    size_t col_ = 1;
};

inline void Lexer::jump_next_line()
{
    line_ += 1;
    col_ = 1;
}

inline char Lexer::peek() const
{
    if (pos_ > script_source_.size())
    {
        return '\0';
    }

    return script_source_[pos_];
}

inline char Lexer::peek_next() const
{
    if (pos_ + 1> script_source_.size())
    {
        return '\0';
    }

    return script_source_[pos_ + 1];
}

inline bool Lexer::eof() const
{
    return pos_ >= script_source_.size();
}

inline void Lexer::skip_ignorable()
{
    while (!eof())
    {
        char c = peek();

        // Skip whitespace, tabs, windows returns.
        if (c == ' ' || c == '\t' || c == '\r')
        {
            advance();
            continue;
        }

        if (c == '\n')
        {
            advance();
            continue;
        }

        // handle comments
        if (c == '/' && peek_next() == '/')
        {
            advance();
            advance();

            while (!eof() && peek() != '\n')
            {
                advance();
            }

            continue;
        }

        // No more characters can be ignored.
        break;
    }
}

inline Token Lexer::make_token(TokenType type,
                               std::string text,
                               size_t line,
                               size_t column)
{
    return Token{type, text, line, column};
}
