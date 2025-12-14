#include "lexer.h"

Lexer::Lexer(std::string source)
:script_source_(std::move(source))
{
}

ParseResult Lexer::tokenize(std::vector<Token> & tokens)
{
    tokens.clear();

    // Go through the script_source_ text and create tokens.
    while (!eof())
    {
        skip_ignorable();

        // When we get here, we have hit a character that we care about or eof.
        if (eof())
        {
            break;
        }

        size_t token_line = line_;
        size_t token_col = col_;

        char c = peek();

        // Handle different cases for the token.

        // Opening brace.
        if (c == '{')
        {
            advance();
            tokens.push_back(
                make_token(TokenType::BraceOpen,
                           "{",
                           token_line,
                           token_col));
            continue;
        }

        // Closing brace.
        if (c == '}')
        {
            advance();
            tokens.push_back(
                make_token(TokenType::BraceClosed,
                           "}",
                           token_line,
                           token_col));
            continue;
        }

        // Found any operator.
        if (VALID_TOKEN_OPERATORS.find(c) != std::string::npos)
        {
            // grab c and turn into a string.
            std::string char_as_str(1, advance());
            tokens.push_back(
                make_token(TokenType::Operator,
                           char_as_str,
                           token_line,
                           token_col));
            continue;
        }

        // For values that are text but need to be values and not identifiers
        if (c == '"')
        {
            advance();
            std::string value;
            while (!eof() && peek() != '"')
            {
                char next_c = advance();
                // Escaped quotes.
                if (next_c == '\\' && peek() == '"')
                {
                    advance();
                    value.push_back('"');
                }
                // Escaped $ symbol.
                else if (next_c == '\\' && peek() == '$')
                {
                    advance();
                    value.push_back('$');
                }
                else
                {
                    value.push_back(next_c);
                }
            }

            // If we hit EOF during this, bad parse.
            if (eof())
            {
                advance();

                ParseResult res;
                res.success = false;
                res.reason = "Reached EOF at [line "
                            + std::to_string(line_)
                            + " column "
                            + std::to_string(col_)
                            + "] (expected ending quote)";

                return res;
            }

            // Other quote needs to be consumed as well.
            advance();

            tokens.push_back(
                make_token(TokenType::Value,
                           value,
                           token_line,
                           token_col));

            // Keep parsing.
            continue;
        }

        // Identifier or keyword (both require alphabetical).
        if (std::isalpha(static_cast<unsigned char>(c)))
        {
            // Store the identifier, mark as keyword if we find out it is one later.
            std::string ident_str;
            while (!eof())
            {
                char next_c = peek();

                // We allow identifiers to have alphanumeric characters or underscores.
                // TODO: document this.
                if (std::isalnum(static_cast<unsigned char>(next_c)) || next_c == '_')
                {
                    ident_str.push_back(advance());
                }
                else
                {
                    break;
                }
            }

            // Determine if our identifier string is a keyword or just an identifier.
            if (VALID_KEYWORDS.find(ident_str) != VALID_KEYWORDS.end())
            {
                tokens.push_back(
                    make_token(TokenType::Keyword,
                               ident_str,
                               token_line,
                               token_col));
            }
            else
            {
                tokens.push_back(
                    make_token(TokenType::Identity,
                               ident_str,
                               token_line,
                               token_col));
            }

            // Go back to parsing the script.
            continue;
        }

        // If the token is a value, handle this. Each value must start with a number.
        if (std::isdigit(static_cast<unsigned char>(c)))
        {
            std::string value;
            while (!eof())
            {
                char next_c = peek();

                if (std::isalnum(static_cast<unsigned char>(next_c)) || next_c == '.')
                {
                    value.push_back(advance());
                }
                else
                {
                    break;
                }
            }

            tokens.push_back(
                make_token(TokenType::Value,
                           value,
                           token_line,
                           token_col));

            // Keep parsing.
            continue;
        }


        // If we do not recognize the token, we should stop now,
        // there is something wrong with the script.
        {
            char bad_c = advance();

            ParseResult res;
            res.success = false;
            res.reason = "Invalid character '"
                         + std::string(1, bad_c)
                         + "' at [line "
                         + std::to_string(line_)
                         + " column "
                         + std::to_string(col_)
                         + "]";

            return res;
        }

    }

    // If we get here, everything was tokenized.
    ParseResult res;
    res.success = true;
    res.reason.clear();

    return res;
}

char Lexer::advance()
{
    if (pos_ >= script_source_.size())
    {
        return '\0';
    }

    char c = script_source_[pos_];
    pos_++;

    if (c == '\n')
    {
        jump_next_line();
    }
    else
    {
        col_ += 1;
    }

    return c;
}
