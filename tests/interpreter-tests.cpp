#include <gtest/gtest.h>

#include "test-helpers.h"

#include "lexer.h"
#include "parser.h"
#include "interpreter.h"

#include "diagnostic-colors.h"

TEST(InterpreterTests, SimpleValidScript)
{
    std::string script_file = "tests/scripts/simple-valid-script.ldsh";

    // Create a known good set of data we expect from parsing this script.
    DSLData correct_data;

    {
        auto & settings = correct_data.settings;
        auto & orchestrator = correct_data.orchestrator;

        // Fill the settings.
        settings.identifier = "my_settings";
        settings.session_protocol = "TCP";

        settings.header_size = 8;
        settings.body_max = 12288;
        settings.read = true;
        settings.repeat = false;

        settings.shards = 4;

        settings.handler_value = "tests/modules/tcp-single-session-heartbeat.wasm";
        settings.endpoints = {"localhost", "127.0.0.1"};

        settings.packet_identifiers["p1"] = "tests/packets/test-packet-1.bin";
        settings.packet_identifiers["p2"] = "tests/packets/test-packet-heavy.bin";

        // Fill the orchestrator
        orchestrator.settings_identifier = "my_settings";

        // We know what these actions should be from the script.

        // CREATE 100 OFFSET 0ms
        {
            Action action_create;

            action_create.type = ActionType::CREATE;
            action_create.range = {0, 100};
            action_create.count = 100;
            action_create.offset_ms = 0;

            orchestrator.actions.push_back(action_create);
        }

        // CONNECT 0:50 OFFSET 100ms
        {
            Action action_connect;

            action_connect.type = ActionType::CONNECT;
            action_connect.range = {0, 50};
            action_connect.count = 50;
            action_connect.offset_ms = 100;

            orchestrator.actions.push_back(action_connect);
        }

        // CONNECT 50:100
        {
            Action action_connect;

            action_connect.type = ActionType::CONNECT;
            action_connect.range = {50, 100};
            action_connect.count = 50;
            action_connect.offset_ms = Parser::DEFAULT_OFFSET_MS;

            orchestrator.actions.push_back(action_connect);
        }

        // SEND 0:100 p1 COPIES 5 TIMESTAMP 0:8 "little":"seconds" OFFSET 200ms
        {
            Action action_send;

            action_send.type = ActionType::SEND;
            action_send.range = {0, 100};
            action_send.packet_identifier = "p1";
            action_send.count = 5;

            TimestampModification time_mod;

            time_mod.timestamp_bytes = {0, 8};
            time_mod.little_endian = true;
            time_mod.format_name = "seconds";

            action_send.push_modifier(time_mod);

            action_send.offset_ms = 200;

            orchestrator.actions.push_back(action_send);
        }

        // SEND 0:100 p1 COPIES 5 COUNTER 0:8 "little":1 OFFSET 200ms
        {
            Action action_send;

            action_send.type = ActionType::SEND;
            action_send.range = {0, 100};
            action_send.packet_identifier = "p1";
            action_send.count = 5;

            CounterModification counter_mod;

            counter_mod.counter_bytes = {0, 8};
            counter_mod.little_endian = true;
            counter_mod.counter_step = 1;

            action_send.push_modifier(counter_mod);

            action_send.offset_ms = 200;

            orchestrator.actions.push_back(action_send);
        }

        // SEND 0:100 p1 COPIES 1
        {
            Action action_send;

            action_send.type = ActionType::SEND;
            action_send.range = {0, 100};
            action_send.packet_identifier = "p1";
            action_send.count = 1;

            action_send.offset_ms = Parser::DEFAULT_OFFSET_MS;

            orchestrator.actions.push_back(action_send);
        }

        // SEND 0:100 p2 COPIES 1 COUNTER 0:8 "little":7
        //      TIMESTAMP 12:8 "big":"milliseconds" OFFSET 200ms
        {
            Action action_send;

            action_send.type = ActionType::SEND;
            action_send.range = {0, 100};
            action_send.packet_identifier = "p2";
            action_send.count = 1;

            CounterModification counter_mod;

            counter_mod.counter_bytes = {0, 8};
            counter_mod.little_endian = true;
            counter_mod.counter_step = 7;

            action_send.push_modifier(counter_mod);

            TimestampModification time_mod;

            time_mod.timestamp_bytes = {12, 8};
            time_mod.little_endian = false;
            time_mod.format_name = "milliseconds";

            action_send.push_modifier(time_mod);

            action_send.offset_ms = 200;

            orchestrator.actions.push_back(action_send);
        }

        // FLOOD 0:100 OFFSET 100ms
        {
            Action action_flood;

            action_flood.type = ActionType::FLOOD;
            action_flood.range = {0, 100};
            action_flood.offset_ms = 100;

            orchestrator.actions.push_back(action_flood);
        }

        // DRAIN 0:100 TIMEOUT 10000ms OFFSET 500ms
        {
            Action action_drain;

            action_drain.type = ActionType::DRAIN;
            action_drain.range = {0, 100};
            action_drain.offset_ms = 500;

            action_drain.count = 10000;

            orchestrator.actions.push_back(action_drain);
        }

        // DISCONNECT 0:100 OFFSET 15s
        {
            Action action_disconnect;

            action_disconnect.type = ActionType::DISCONNECT;
            action_disconnect.range = {0, 100};
            action_disconnect.offset_ms = 15000;

            orchestrator.actions.push_back(action_disconnect);
        }
    }

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
