#pragma once

#include "parse-result.h"
#include "token.h"
#include "script-structs.h"

class Parser
{
public:
    // Default to 250ms offsets.
    static constexpr uint32_t DEFAULT_OFFSET_MS = 250;

    // Default to 10s drain timeout.
    static constexpr uint32_t DEFAULT_TIMEOUT_MS = 10 * 1000;

public:
    Parser(const std::vector<Token> & tokens);

    ParseResult parse(DSLData & script_data);

private:
    ParseResult parse_settings(SettingsBlock & settings);

    ParseResult parse_orchestrator(OrchestratorBlock & orchestrator);

    // See if the current index is past the end of tokens.
    bool end_of_tokens();

    // Increment and return previous.
    Token consume();

    // See the current token.
    Token peek();

    bool is_expected(TokenType expected);

    ParseResult try_convert_int(const Token & t,
                                uint32_t & res,
                                const std::string & keyword);

    // Try to parse [:][Value] from the token sequence, then convert.
    ParseResult try_parse_range(const Token & first_value,
                                Range & range,
                                const std::string & keyword);

    ParseResult try_parse_time(const Token & time_token,
                               uint32_t & time_ms);

    // Defined in this header to satisfy ODR.
    template<ModificationType M>
    ParseResult try_parse_send_option(Action & action);

    // ParseResult related utilities.
    ParseResult arbitrary_error(std::string reason);

    ParseResult bad_type_error(Token t, TokenType expected);

    ParseResult bad_action_error(Token t);

    ParseResult bad_operator_error(Token t, std::string op);

    ParseResult bad_integer_error(Token t, std::string keyword);

    ParseResult negative_integer_error(Token t);

    ParseResult bad_time_error(Token t);

    ParseResult bad_time_format(Token t);

    ParseResult bad_bool_error(Token t);

    ParseResult bad_endian_error(Token t);

    ParseResult bad_nesting_error(Token t);

    ParseResult missing_copies_error(Token t);

    ParseResult unterminated_error(Token t);

    ParseResult good_parse();

private:
    const std::vector<Token> & input_tokens_;
    size_t token_pos_{0};
};

template<ModificationType M>
ParseResult Parser::try_parse_send_option(Action & action)
{
    //
    // Deal with timestamp data.
    //
    if constexpr (M == ModificationType::Timestamp)
    {
        // We expect a range, followed by [Value] [:] [Value]
        TimestampModification time_mod;

        // Grab the first token and call to parse range.
        if (!is_expected(TokenType::Value))
        {
            Token temp = peek();
            return bad_type_error(temp, TokenType::Value);
        }

        Token first_value = consume();

        ParseResult range_res = try_parse_range(first_value,
                                                time_mod.timestamp_bytes,
                                                "TIMESTAMP");
        if (!range_res.success)
        {
            return range_res;
        }

        // Now, we need to do the same thing but pull out values.
        if (!is_expected(TokenType::Value))
        {
            Token temp = peek();
            return bad_type_error(temp, TokenType::Value);
        }

        Token first_arg_val = consume();

        if (!is_expected(TokenType::Operator) || peek().text != ":")
        {
            Token temp = peek();
            return bad_operator_error(temp, ":");
        }

        // Eat the : operator
        consume();

        if (!is_expected(TokenType::Value))
        {
            Token temp = peek();
            return bad_type_error(temp, TokenType::Value);
        }

        Token second_arg_val = consume();

        // We now need to convert these two arguments for the timestamp mod.

        // Try to extract the endian-ness
        if (first_arg_val.text == "little")
        {
            time_mod.little_endian = true;
        }
        else if (first_arg_val.text == "big")
        {
            time_mod.little_endian = false;
        }
        else
        {
            return bad_endian_error(first_arg_val);
        }

        // Try to extract the time.
        if (second_arg_val.text == "seconds"
            || second_arg_val.text == "milliseconds"
            || second_arg_val.text == "microseconds"
            || second_arg_val.text == "nanoseconds")
        {
            time_mod.format_name = second_arg_val.text;
        }
        else
        {
            return bad_time_format(second_arg_val);
        }

        // At this point, we have good data, push this and return.
        action.push_modifier(time_mod);
        return good_parse();
    }
    //
    // Deal with counter data.
    //
    else if constexpr (M == ModificationType::Counter)
    {
        // We expect a range, followed by [Value] [:] [Value]
        CounterModification counter_mod;

        // Grab the first token and call to parse range.
        if (!is_expected(TokenType::Value))
        {
            Token temp = peek();
            return bad_type_error(temp, TokenType::Value);
        }

        Token first_value = consume();

        ParseResult range_res = try_parse_range(first_value,
                                                counter_mod.counter_bytes,
                                                "TIMESTAMP");
        if (!range_res.success)
        {
            return range_res;
        }

        // Now, we need to do the same thing but pull out values.
        if (!is_expected(TokenType::Value))
        {
            Token temp = peek();
            return bad_type_error(temp, TokenType::Value);
        }

        Token first_arg_val = consume();

        if (!is_expected(TokenType::Operator) || peek().text != ":")
        {
            Token temp = peek();
            return bad_operator_error(temp, ":");
        }

        // Eat the : operator
        consume();

        if (!is_expected(TokenType::Value))
        {
            Token temp = peek();
            return bad_type_error(temp, TokenType::Value);
        }

        Token second_arg_val = consume();

        // We now need to convert these two arguments for the timestamp mod.

        // Try to extract the endian-ness
        if (first_arg_val.text == "little")
        {
            counter_mod.little_endian = true;
        }
        else if (first_arg_val.text == "big")
        {
            counter_mod.little_endian = false;
        }
        else
        {
            return bad_endian_error(first_arg_val);
        }

        // Extract the step.
        ParseResult int_res = try_convert_int(second_arg_val,
                                              counter_mod.counter_step,
                                              "COUNTER");
        if (!int_res.success)
        {
            return int_res;
        }

        // At this point, we have good data, push this and return.
        action.push_modifier(counter_mod);
        return good_parse();
    }
    // Bad usage.
    else
    {
        static_assert(always_false<decltype(M)>,
                    "try_parse_send_option() not implemented for this type");
    }
}
