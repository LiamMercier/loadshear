#include "parser.h"

#include <iostream>

Parser::Parser(const std::vector<Token> & tokens)
:input_tokens_(tokens)
{
}

ParseResult Parser::parse(DSLData & script_data)
{
    while (!end_of_tokens())
    {
        Token t = peek();

        // Top level, parse one of the two blocks permitted.
        if (t.text == "SETTINGS")
        {
            ParseResult settings_res = parse_settings(script_data.settings);

            if (!settings_res.success)
            {
                return settings_res;
            }
        }
        else if (t.text == "ORCHESTRATOR")
        {
            ParseResult orchestrator_res = parse_orchestrator(script_data.orchestrator);

            if (!orchestrator_res.success)
            {
                return orchestrator_res;
            }
        }
        else
        {
            std::string error_msg = "Unexpected token "
                                    + t.text
                                    + " of type "
                                    + ttype_to_string(t.type)
                                    + " at [line "
                                    + std::to_string(t.line)
                                    + " column "
                                    + std::to_string(t.col)
                                    + "] (expected SETTINGS or ORCHESTRATOR)";

            ParseResult res;
            res.success = false;
            res.reason = std::move(error_msg);
            return res;
        }
    }

    // If we get here, everything was parsed correctly.
    ParseResult res;
    res.success = true;
    res.reason.clear();

    return res;
}

ParseResult Parser::parse_settings(SettingsBlock & settings)
{
    // Consume the SETTINGS token
    consume();

    // We expect the next token to be an Identity naming this settings block.
    if (!is_expected(TokenType::Identity))
    {
        Token temp = peek();
        return bad_type_error(temp, TokenType::Identity);
    }

    settings.identifier = peek().text;

    // Consume the name token.
    consume();

    // Next, we expect a brace.
    if (!is_expected(TokenType::BraceOpen))
    {
        Token temp = peek();
        return bad_type_error(temp, TokenType::BraceOpen);
    }

    // Consume the brace.
    consume();

    // Process the SETTINGS block in a loop until we find a closing brace.
    while (!is_expected(TokenType::BraceClosed))
    {
        // We expect each loop to start with a keyword (ENDPOINTS, READ, SHARDS, etc).
        if (!is_expected(TokenType::Keyword))
        {
            Token temp = peek();
            return bad_type_error(temp, TokenType::Keyword);
        }

        Token keyword = consume();

        // Now, handle the type of keyword.

        //
        // If we are in a nested { } block (ENDPOINTS, PACKETS)
        //
        if (is_expected(TokenType::BraceOpen))
        {
            // Consume the {
            consume();

            // If the keyword was PACKETS then handle named packets.
            if (keyword.text == "PACKETS")
            {
                // Parse named packets in a loop until we find }
                while (!is_expected(TokenType::BraceClosed))
                {
                    // We expect [Identity] [Operator (:)] [Value] [Operator (,)] each loop.

                    if (!is_expected(TokenType::Identity))
                    {
                        Token temp = peek();
                        return bad_type_error(temp, TokenType::Identity);
                    }

                    Token packet_identity = consume();

                    if (!is_expected(TokenType::Operator) || peek().text != ":")
                    {
                        Token temp = peek();
                        return bad_operator_error(temp, ":");
                    }

                    // Eat the : token.
                    consume();

                    if (!is_expected(TokenType::Value))
                    {
                        Token temp = peek();
                        return bad_type_error(temp, TokenType::Value);
                    }

                    Token packet_value = consume();

                    // Get data into our packets list.
                    settings.packet_identifiers[packet_identity.text] = packet_value.text;

                    if (!is_expected(TokenType::Operator) || peek().text != ",")
                    {
                        // Break if this is our enclosing brace
                        if (is_expected(TokenType::BraceClosed))
                        {
                            break;
                        }

                        Token temp = peek();
                        return bad_operator_error(temp, ",");
                    }

                    // Consume the comma
                    consume();
                }
            }

            // The other option if we have an open brace is the ENDPOINTS keyword.
            else if (keyword.text == "ENDPOINTS")
            {
                // Parse named packets in a loop until we find }
                while (!is_expected(TokenType::BraceClosed))
                {
                    // We expect [Value] [Operator (,)] each loop.
                    if (!is_expected(TokenType::Value))
                    {
                        Token temp = peek();
                        return bad_type_error(temp, TokenType::Value);
                    }

                    Token endpoint_value = consume();

                    // Get this data into our list.
                    settings.endpoints.push_back(endpoint_value.text);

                    if (!is_expected(TokenType::Operator) || peek().text != ",")
                    {
                        // Break if this is our enclosing brace
                        if (is_expected(TokenType::BraceClosed))
                        {
                            break;
                        }

                        Token temp = peek();
                        return bad_operator_error(temp, ",");
                    }

                    // Consume the comma.
                    consume();
                }
            }
            // Handle nested data with wrong keyword.
            else
            {
                Token temp = peek();
                return bad_nesting_error(temp);
            }

            // Consume the closing brace }
            if (!is_expected(TokenType::BraceClosed))
            {
                Token temp = peek();
                return unterminated_error(temp);
            }

            consume();
        }
        else
        {
            // If the keyword is for standard data (no nesting) then we
            // expect [Operator (=)] [Value]

            if (!is_expected(TokenType::Operator) || peek().text != "=")
            {
                Token temp = peek();
                return bad_operator_error(temp, "=");
            }

            consume();

            if (!is_expected(TokenType::Value))
            {
                Token temp = peek();
                return bad_type_error(temp, TokenType::Value);
            }

            Token value_token = consume();

            // Figure out what value this corresponds to.
            if (keyword.text == "SESSION")
            {
                // For the session field, we just copy the text (TCP, UDP, etc).
                settings.session_protocol = value_token.text;
            }
            else if (keyword.text == "HEADERSIZE")
            {
                // Try to convert to integer, fail otherwise.
                try
                {
                    settings.header_size = std::stoi(value_token.text);
                }
                catch (const std::exception & error)
                {
                    // Return error
                    return bad_integer_error(value_token, "HEADERSIZE");
                }
            }
            else if (keyword.text == "BODYMAX")
            {
                // Try to convert to integer, fail otherwise.
                try
                {
                    settings.body_max = std::stoi(value_token.text);
                }
                catch (const std::exception & error)
                {
                    // Return error
                    return bad_integer_error(value_token, "BODYMAX");
                }
            }
            else if (keyword.text == "READ")
            {
                if (value_token.text == "true")
                {
                    settings.read = true;
                }
                else if (value_token.text == "false")
                {
                    settings.read = false;
                }
                else
                {
                    return bad_bool_error(value_token);
                }
            }
            else if (keyword.text == "REPEAT")
            {
                if (value_token.text == "true")
                {
                    settings.repeat = true;
                }
                else if (value_token.text == "false")
                {
                    settings.repeat = false;
                }
                else
                {
                    return bad_bool_error(value_token);
                }
            }
            else if (keyword.text == "SHARDS")
            {
                // Try to convert to integer, fail otherwise.
                try
                {
                    settings.shards = std::stoi(value_token.text);
                }
                catch (const std::exception & error)
                {
                    // Return error
                    return bad_integer_error(value_token, "SHARDS");
                }
            }
            else if (keyword.text == "HANDLER")
            {
                settings.handler_value = value_token.text;
            }
            else
            {
                return arbitrary_error("Bad keyword detected? This is a software bug.");
            }
        }
    }

    // End of SETTINGS loop, we expect a closing brace.
    if (!is_expected(TokenType::BraceClosed))
    {
        Token temp = peek();
        return bad_type_error(temp, TokenType::BraceClosed);
    }

    consume();

    ParseResult res;
    res.success = true;
    res.reason.clear();

    return res;
}

// TODO: parse orchestrator grammar.
ParseResult Parser::parse_orchestrator(OrchestratorBlock & orchestrator)
{
    return {0, "parse_orchestrator has not been implemented."};
}

bool Parser::end_of_tokens()
{
    return token_pos_ >= input_tokens_.size();
}

// Increment the token position and return the token we went over.
Token Parser::consume()
{
    if (!end_of_tokens())
    {
        token_pos_++;
    }

    auto t = input_tokens_[token_pos_ - 1];
    std::cerr << "Consuming "
              << "[" << ttype_to_string(t.type)
              << ", " << t.text
              << ", " << t.line << ":" << t.col << "]\n";

    return input_tokens_[token_pos_ - 1];
}

Token Parser::peek()
{
    return input_tokens_[token_pos_];
}

bool Parser::is_expected(TokenType expected)
{
    if (end_of_tokens())
    {
        return false;
    }

    return (peek().type == expected);
}

ParseResult Parser::arbitrary_error(std::string reason)
{
    ParseResult res;
    res.success = false;
    res.reason = std::move(reason);
    return res;
}

ParseResult Parser::bad_type_error(Token t, TokenType expected)
{
    std::string error_msg = "Unexpected token "
                            + t.text
                            + " of type "
                            + ttype_to_string(t.type)
                            + " at [line "
                            + std::to_string(t.line)
                            + " column "
                            + std::to_string(t.col)
                            + "] (expected type "
                            + ttype_to_string(expected)
                            + ")";

    return arbitrary_error(error_msg);
}

ParseResult Parser::bad_operator_error(Token t, std::string op)
{
    std::string error_msg = "Unexpected token "
                            + t.text
                            + " of type "
                            + ttype_to_string(t.type)
                            + " at [line "
                            + std::to_string(t.line)
                            + " column "
                            + std::to_string(t.col)
                            + "] (expected operator "
                            + op
                            + ")";

    return arbitrary_error(error_msg);
}

ParseResult Parser::bad_integer_error(Token t, std::string keyword)
{
    std::string error_msg = "Unexpected token "
                            + t.text
                            + " of type "
                            + ttype_to_string(t.type)
                            + " at [line "
                            + std::to_string(t.line)
                            + " column "
                            + std::to_string(t.col)
                            + "] (expected an integer value for "
                            + keyword
                            + ")";

    return arbitrary_error(error_msg);
}

ParseResult Parser::bad_bool_error(Token t)
{
    std::string error_msg = "Unexpected token "
                            + t.text
                            + " of type "
                            + ttype_to_string(t.type)
                            + " at [line "
                            + std::to_string(t.line)
                            + " column "
                            + std::to_string(t.col)
                            + "] (expected true or false)";

    return arbitrary_error(error_msg);
}

ParseResult Parser::bad_nesting_error(Token t)
{
    std::string error_msg = "Unexpected token "
                            + t.text
                            + " of type "
                            + ttype_to_string(t.type)
                            + " at [line "
                            + std::to_string(t.line)
                            + " column "
                            + std::to_string(t.col)
                            + "] (unexpected nesting)";

    return arbitrary_error(error_msg);
}

ParseResult Parser::unterminated_error(Token t)
{
    std::string error_msg = "Unexpected token "
                            + t.text
                            + " of type "
                            + ttype_to_string(t.type)
                            + " at [line "
                            + std::to_string(t.line)
                            + " column "
                            + std::to_string(t.col)
                            + "] (expected nesting terminator } instead)";

    return arbitrary_error(error_msg);
}
