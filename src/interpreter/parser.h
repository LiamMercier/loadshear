#pragma once

#include "parse-result.h"
#include "token.h"
#include "script-structs.h"

class Parser
{
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

    // Try to parse [:][Value] from the token sequence, then convert.
    ParseResult try_parse_range(const Token & first_value,
                                Range & range,
                                const std::string & keyword);

    ParseResult try_parse_time(const Token & time_token,
                               uint32_t & time_ms);

    // ParseResult related utilities.
    ParseResult arbitrary_error(std::string reason);

    ParseResult bad_type_error(Token t, TokenType expected);

    ParseResult bad_action_error(Token t);

    ParseResult bad_operator_error(Token t, std::string op);

    ParseResult bad_integer_error(Token t, std::string keyword);

    ParseResult bad_time_error(Token t);

    ParseResult bad_bool_error(Token t);

    ParseResult bad_nesting_error(Token t);

    ParseResult unterminated_error(Token t);

private:
    const std::vector<Token> & input_tokens_;
    size_t token_pos_{0};
};
