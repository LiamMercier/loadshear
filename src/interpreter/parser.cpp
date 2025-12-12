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

            return arbitrary_error(std::move(error_msg));
        }
    }

    // If we get here, everything was parsed correctly.
    return good_parse();
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

                ParseResult int_res = try_convert_int(value_token,
                                                      settings.header_size,
                                                      "HEADERSIZE");
                if (!int_res.success)
                {
                    return int_res;
                }
            }
            else if (keyword.text == "BODYMAX")
            {
                // Try to convert to integer, fail otherwise.
                ParseResult int_res = try_convert_int(value_token,
                                                      settings.body_max,
                                                      "BODYMAX");
                if (!int_res.success)
                {
                    return int_res;
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
                ParseResult int_res = try_convert_int(value_token,
                                                      settings.shards,
                                                      "SHARDS");
                if (!int_res.success)
                {
                    return int_res;
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

    return good_parse();
}

ParseResult Parser::parse_orchestrator(OrchestratorBlock & orchestrator)
{
    // Eat the ORCHESTRATOR token
    consume();

    // We expect the next token to be this block's settings identity.
    if (!is_expected(TokenType::Identity))
    {
        Token temp = peek();
        return bad_type_error(temp, TokenType::Identity);
    }

    // Store the setting's identity.
    orchestrator.settings_identifier = consume().text;

    // We expect the next token to be an open brace.
    if (!is_expected(TokenType::BraceOpen))
    {
        Token temp = peek();
        return bad_type_error(temp, TokenType::BraceOpen);
    }

    // Consume the brace.
    consume();

    // Now, parse the action's in the ORCHESTRATOR block until we find a closing brace.
    while (!is_expected(TokenType::BraceClosed))
    {
        // Our action lines (except CREATE) look like
        // [KEYWORD (ACTION)] [VALUE] <OPTIONALS> [KEYWORD (OFFSET)] [Value]

        // First, grab the keyword for this action so we know what to do
        if (!is_expected(TokenType::Keyword))
        {
            Token temp = peek();
            return bad_type_error(temp, TokenType::Keyword);
        }

        // Save the token for this action.
        Token t_action = consume();
        Action action;

        // Each of these keywords must have a value following it.
        if (!is_expected(TokenType::Value))
        {
            Token temp = peek();
            return bad_type_error(temp, TokenType::Value);
        }

        Token first_val = consume();

        if (t_action.text == "SEND")
        {
            action.type = ActionType::SEND;

            ParseResult range_res = try_parse_range(first_val,
                                                    action.range,
                                                    t_action.text);

            if (!range_res.success)
            {
                return range_res;
            }

            // Now, we expect the identity of the packet.
            if (!is_expected(TokenType::Identity))
            {
                Token temp = peek();
                return bad_type_error(temp, TokenType::Identity);
            }

            Token packet_id = consume();

            // Save this ID in the action.
            action.packet_identifier = packet_id.text;

            // We expect COPIES next.
            if (!is_expected(TokenType::Keyword))
            {
                Token temp = peek();
                return bad_type_error(temp, TokenType::Keyword);
            }
            else if (peek().text != "COPIES")
            {
                Token temp = peek();
                return missing_copies_error(temp);
            }

            // Consume COPIES
            consume();

            // We expect a valid integer Value now.
            if (!is_expected(TokenType::Value))
            {
                Token temp = peek();
                return bad_type_error(temp, TokenType::Value);
            }

            Token copies_value = consume();

            // Try to turn this into a valid integer.
            ParseResult int_res = try_convert_int(copies_value,
                                                  action.count,
                                                  "COPIES");
            if (!int_res.success)
            {
                return int_res;
            }

            // Now, handle optional fields.

            // If we hit an OFFSET keyword or a closing brace, stop looking for modifications.
            while (peek().text != "OFFSET"
                   && is_expected(TokenType::Keyword))
            {
                // Deal with COUNTER
                if (peek().text == "COUNTER")
                {
                    consume();

                    // Try to parse SEND options specialized on
                    // the ModificationType we pass.
                    ParseResult res = try_parse_send_option<ModificationType::Counter>(action);

                    if (!res.success)
                    {
                        return res;
                    }
                }
                // Deal with TIMESTAMP
                else if (peek().text == "TIMESTAMP")
                {
                    consume();

                    ParseResult res = try_parse_send_option<ModificationType::Timestamp>(action);

                    if (!res.success)
                    {
                        return res;
                    }
                }
                // Handle no OFFSET but also no further modifications.
                else
                {
                    break;
                }
            }

            // Anything else is an OFFSET, a closing brace, or a bad token
            // we will fail on later. Continue as normal.
        }
        else if (t_action.text == "CONNECT")
        {
            action.type = ActionType::CONNECT;

            ParseResult range_res = try_parse_range(first_val,
                                                    action.range,
                                                    t_action.text);

            if (!range_res.success)
            {
                return range_res;
            }
        }
        else if (t_action.text == "FLOOD")
        {
            action.type = ActionType::FLOOD;

            ParseResult range_res = try_parse_range(first_val,
                                                    action.range,
                                                    t_action.text);

            if (!range_res.success)
            {
                return range_res;
            }
        }
        else if (t_action.text == "DRAIN")
        {
            action.type = ActionType::DRAIN;

            ParseResult range_res = try_parse_range(first_val,
                                                    action.range,
                                                    t_action.text);

            if (!range_res.success)
            {
                return range_res;
            }

            // Grab timeout if it exists.
            if (peek().text == "TIMEOUT")
            {
                consume();

                // See if the next token is a Value token.
                if (!is_expected(TokenType::Value))
                {
                    Token temp = peek();
                    return bad_type_error(temp, TokenType::Value);
                }

                // Save the timeout value.
                Token timeout_token = consume();

                ParseResult time_res = try_parse_time(timeout_token, action.count);

                // Check the result was good.
                if (!time_res.success)
                {
                    return time_res;
                }

                // All fields for drain are done besides OFFSET done at the end of this block.
            }
            // Otherwise, set to default if nothing was specified.
            else
            {
                action.count = DEFAULT_TIMEOUT_MS;
            }

        }
        else if (t_action.text == "DISCONNECT")
        {
            action.type = ActionType::DISCONNECT;

            ParseResult range_res = try_parse_range(first_val,
                                                    action.range,
                                                    t_action.text);

            if (!range_res.success)
            {
                return range_res;
            }
        }
        else if (t_action.text == "CREATE")
        {
            // We simply need to check that the first value is an integer.
            action.type = ActionType::CREATE;

            // Try to convert to integer, fail otherwise.
            ParseResult int_res = try_convert_int(first_val,
                                                  action.count,
                                                  "CREATE");
            if (!int_res.success)
            {
                return int_res;
            }

            action.range.start = 0;
            action.range.second = action.count;
        }
        else
        {
            // Bad token keyword (not an action), report error.
            Token temp = peek();
            return bad_action_error(temp);
        }

        // Finally, check for an offset block and set default if none is found.
        if (peek().text == "OFFSET")
        {
            // Eat the OFFSET
            consume();

            // We expect a time convertible Value token.
            if (!is_expected(TokenType::Value))
            {
                Token temp = peek();
                return bad_type_error(temp, TokenType::Value);
            }

            // Save the timeout value.
            Token timeout_token = consume();

            ParseResult time_res = try_parse_time(timeout_token, action.offset_ms);

            // Give error if we could not convert.
            if (!time_res.success)
            {
                return time_res;
            }

            // Otherwise, we have stored the offset and should move on.
        }
        else
        {
            action.offset_ms = DEFAULT_OFFSET_MS;
        }

        // Push back this action and restart the loop.
        orchestrator.actions.push_back(std::move(action));
    }

    // After we exit the loop, we expect that there was a closing brace.
    if (!is_expected(TokenType::BraceClosed))
    {
        Token temp = peek();
        return bad_type_error(temp, TokenType::BraceClosed);
    }

    consume();

    return good_parse();
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

ParseResult Parser::try_convert_int(const Token & t,
                                    uint32_t & res,
                                    const std::string & keyword)
{
    try
    {
        int pos = std::stoi(t.text);

        if (pos < 0)
        {
            return negative_integer_error(t);
        }

        res = pos;
    }
    catch (const std::exception & error)
    {
        // Return error
        return bad_integer_error(t, keyword);
    }

    return good_parse();
}

ParseResult Parser::try_parse_range(const Token & first_value,
                                    Range & range,
                                    const std::string & keyword)
{
    // We expect the range operator : after the first value.
    if (!is_expected(TokenType::Operator) || peek().text != ":")
    {
        Token temp = peek();
        return bad_operator_error(temp, ":");
    }

    consume();

    // We expect another value to complete the range.
    if (!is_expected(TokenType::Value))
    {
        Token temp = peek();
        return bad_type_error(temp, TokenType::Value);
    }

    // Save this value and then try to convert.
    Token second_val = consume();

    ParseResult first_res = try_convert_int(first_value, range.start, keyword);

    if (!first_res.success)
    {
        return first_res;
    }

    ParseResult second_res = try_convert_int(second_val, range.second, keyword);

    if (!second_res.success)
    {
        return second_res;
    }

    return good_parse();
}

ParseResult Parser::try_parse_time(const Token & time_token,
                                   uint32_t & time_ms)
{
    // Try to turn this token into a uint32_t offset of milliseconds.
    try
    {
        // Check if ms or s, then stoi and multiply if seconds.
        const std::string & time_string = time_token.text;

        uint32_t mul = 1;
        std::string time_int;

        if (time_string.ends_with("ms"))
        {
            // Remove the ms.
            time_int = time_string.substr(0, time_string.size() - 1);
        }
        else if (time_string.ends_with("s"))
        {
            // 1 second = 1000 milliseconds.
            mul = 1000;

            // Remove the s.
            time_int = time_string.substr(0, time_string.size() - 1);
        }
        else
        {
            return bad_time_error(time_token);
        }

        // Now, try to convert, or this is a bad value.
        int pos = std::stoi(time_int);

        if (pos < 0)
        {
            return negative_integer_error(time_token);
        }

        time_ms = (pos * mul);
    }
    catch (const std::exception & error)
    {
        // Return error
        return bad_time_error(time_token);
    }

    return good_parse();
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

ParseResult Parser::bad_action_error(Token t)
{
    std::string error_msg = "Unexpected keyword "
                            + t.text
                            + " at [line "
                            + std::to_string(t.line)
                            + " column "
                            + std::to_string(t.col)
                            + "] (expected a valid action keyword"
                            + " such as CREATE, CONNECT, ... )";

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

ParseResult Parser::negative_integer_error(Token t)
{
    std::string error_msg = "Unexpected token "
                            + t.text
                            + " of type "
                            + ttype_to_string(t.type)
                            + " at [line "
                            + std::to_string(t.line)
                            + " column "
                            + std::to_string(t.col)
                            + "] (expected a positive integer)";

    return arbitrary_error(error_msg);
}

ParseResult Parser::bad_time_error(Token t)
{
    std::string error_msg = "Unexpected token "
                            + t.text
                            + " of type "
                            + ttype_to_string(t.type)
                            + " at [line "
                            + std::to_string(t.line)
                            + " column "
                            + std::to_string(t.col)
                            + "] (expected a valid time value, i.e 1ms, 1s)";

    return arbitrary_error(error_msg);
}

ParseResult Parser::bad_time_format(Token t)
{
    std::string error_msg = "Unexpected token "
                            + t.text
                            + " of type "
                            + ttype_to_string(t.type)
                            + " at [line "
                            + std::to_string(t.line)
                            + " column "
                            + std::to_string(t.col)
                            + "] (expected a valid time format, "
                            + "i.e seconds, milliseconds, microseconds, nanoseconds)";

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

ParseResult Parser::bad_endian_error(Token t)
{
    std::string error_msg = "Unexpected token "
                            + t.text
                            + " of type "
                            + ttype_to_string(t.type)
                            + " at [line "
                            + std::to_string(t.line)
                            + " column "
                            + std::to_string(t.col)
                            + "] (expected little or big for endian field)";

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

ParseResult Parser::missing_copies_error(Token t)
{
    std::string error_msg = "Unexpected token "
                            + t.text
                            + " of type "
                            + ttype_to_string(t.type)
                            + " at [line "
                            + std::to_string(t.line)
                            + " column "
                            + std::to_string(t.col)
                            + "] (expected COPIES instead)";

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

ParseResult Parser::good_parse()
{
    ParseResult res;
    res.success = true;
    res.reason.clear();

    return res;
}
