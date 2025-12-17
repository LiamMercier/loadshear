#include <gtest/gtest.h>

#include "test-helpers.h"
#include "test-fixtures.h"

#include "lexer.h"
#include "parser.h"
#include "interpreter.h"

TEST(InterpreterTests, SimpleValidScript)
{
    std::string script_file = "tests/scripts/simple-valid-script.ldsh";

    // Create a known good set of data we expect from parsing this script.
    DSLData correct_data = get_simple_valid_script_data();

    Interpreter interpreter;

    ParseResult result = interpreter.parse_script(script_file);

    EXPECT_DSL_EQ(correct_data, interpreter.script_);

    EXPECT_TRUE(result.success) << "Parsing failed: "
                                << result.reason;

}

TEST(InterpreterTests, BadCountersScript)
{
    std::string script_file = "tests/scripts/bad-counter-script.loadshear";

    Interpreter interpreter;

    ParseResult result = interpreter.parse_script(script_file);

    EXPECT_FALSE(result.success) << "Parse result was successful for a known "
                                 << "invalid script ("
                                 << script_file
                                 << ")";
}

TEST(InterpreterTests, BadPacketsBlock)
{
    std::string script_file = "tests/scripts/bad-packets-script.ldsh";

    Interpreter interpreter;

    ParseResult result = interpreter.parse_script(script_file);

    EXPECT_FALSE(result.success) << "Parse result was successful for a known "
                                 << "invalid script ("
                                 << script_file
                                 << ")";
}

TEST(InterpreterTests, FileEndsEarly)
{
    std::string script_file = "tests/scripts/broken-script.loadshear";

    Interpreter interpreter;

    ParseResult result = interpreter.parse_script(script_file);

    EXPECT_FALSE(result.success) << "Parse result was successful for a known "
                                 << "invalid script ("
                                 << script_file
                                 << ")";
}

TEST(InterpreterTests, BadParseType)
{
    std::string script_file = "tests/scripts/bad-type-script.ldsh";

    Interpreter interpreter;

    ParseResult result = interpreter.parse_script(script_file);

    EXPECT_FALSE(result.success) << "Parse result was successful for a known "
                                 << "invalid script ("
                                 << script_file
                                 << ")";
}

TEST(ParserTests, ParserFailures)
{
    std::vector<Token> input_tokens;

    input_tokens.push_back(Token{TokenType::Keyword, "SETTINGS", 0, 0});
    input_tokens.push_back(Token{TokenType::Identity, "my_settings", 0, 0});
    input_tokens.push_back(Token{TokenType::BraceOpen, "{", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "SESSION", 0, 0});
    input_tokens.push_back(Token{TokenType::Operator, "=", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "TCP", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "HEADERSIZE", 0, 0});
    input_tokens.push_back(Token{TokenType::Operator, "=", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "8", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "BODYMAX", 0, 0});
    input_tokens.push_back(Token{TokenType::Operator, "=", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "16", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "READ", 0, 0});
    input_tokens.push_back(Token{TokenType::Operator, "=", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "true", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "REPEAT", 0, 0});
    input_tokens.push_back(Token{TokenType::Operator, "=", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "true", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "ENDPOINTS", 0, 0});
    input_tokens.push_back(Token{TokenType::BraceOpen, "{", 0, 0});

    input_tokens.push_back(Token{TokenType::Value, "localhost", 0, 0});
    input_tokens.push_back(Token{TokenType::Operator, ",", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "127.0.0.1", 0, 0});

    input_tokens.push_back(Token{TokenType::BraceClosed, "}", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "SHARDS", 0, 0});
    input_tokens.push_back(Token{TokenType::Operator, "=", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "8", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "PACKETS", 0, 0});
    input_tokens.push_back(Token{TokenType::BraceOpen, "{", 0, 0});

    input_tokens.push_back(Token{TokenType::Identity, "p1", 0, 0});
    input_tokens.push_back(Token{TokenType::Operator, ":", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "unresolved", 0, 0});

    input_tokens.push_back(Token{TokenType::Operator, ",", 0, 0});

    input_tokens.push_back(Token{TokenType::Identity, "p2", 0, 0});
    input_tokens.push_back(Token{TokenType::Operator, ":", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "unresolved2", 0, 0});

    input_tokens.push_back(Token{TokenType::BraceClosed, "}", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "HANDLER", 0, 0});
    input_tokens.push_back(Token{TokenType::Operator, "=", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "badwasm.wasm", 0, 0});

    input_tokens.push_back(Token{TokenType::BraceClosed, "}", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "ORCHESTRATOR", 0, 0});
    input_tokens.push_back(Token{TokenType::Identity, "my_settings", 0, 0});
    input_tokens.push_back(Token{TokenType::BraceOpen, "{", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "CREATE", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "100", 0, 0});
    input_tokens.push_back(Token{TokenType::Keyword, "OFFSET", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "0ms", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "CONNECT", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "0", 0, 0});
    input_tokens.push_back(Token{TokenType::Operator, ":", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "100", 0, 0});
    input_tokens.push_back(Token{TokenType::Keyword, "OFFSET", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "100ms", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "SEND", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "0", 0, 0});
    input_tokens.push_back(Token{TokenType::Operator, ":", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "100", 0, 0});
    input_tokens.push_back(Token{TokenType::Identity, "p1", 0, 0});
    input_tokens.push_back(Token{TokenType::Keyword, "COPIES", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "1", 0, 0});
    input_tokens.push_back(Token{TokenType::Keyword, "OFFSET", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "100ms", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "FLOOD", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "0", 0, 0});
    input_tokens.push_back(Token{TokenType::Operator, ":", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "100", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "DRAIN", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "0", 0, 0});
    input_tokens.push_back(Token{TokenType::Operator, ":", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "100", 0, 0});
    input_tokens.push_back(Token{TokenType::Keyword, "TIMEOUT", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "10s", 0, 0});

    input_tokens.push_back(Token{TokenType::Keyword, "DISCONNECT", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "0", 0, 0});
    input_tokens.push_back(Token{TokenType::Operator, ":", 0, 0});
    input_tokens.push_back(Token{TokenType::Value, "110", 0, 0});

    input_tokens.push_back(Token{TokenType::BraceClosed, "}", 0, 0});

    // Go through the tokens, skipping one more each time (first N, N-1, N-2...).
    // We should fail every time except when we have no orchestrator block.

    size_t total = input_tokens.size();

    for (size_t i = 0; i < total - 1; i++)
    {
        input_tokens.pop_back();

        Parser this_parser(input_tokens);

        DSLData data;

        auto res = this_parser.parse(data);

        // At this index we have no orchestrator block but we do have
        // an entire settings block, so we wont return an error at time parse.
        if (i != 36)
        {
            EXPECT_FALSE(res.success) << "Parse returned success for bad script!";
        }
    }

}
