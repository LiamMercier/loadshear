#include <gtest/gtest.h>

#include "test-helpers.h"

#include "interpreter.h"

TEST(InterpreterTests, SimpleValidScript)
{
    std::filesystem::path script_file = "tests/scripts/simple-valid-script.ldsh";

    // Create a known good set of data we expect from parsing this script.
    DSLData correct_data;

    {
        auto & settings = correct_data.settings;
        auto & ochestrator = correct_data.orchestrator;

        // Fill the settings.
        settings.identifier = "my_settings";
        settings.session_protocol = "TCP";

        settings.header_size = 8;
        settings.body_max = 12288;
        settings.read = true;
        settings.repeat = false;

        settings.shards = 4;

        settings.handler_value = "test-module.wasm";
        settings.endpoints = {"localhost", "127.0.0.1"};

        settings.packet_identifiers["p1"] = "test-packet-1.bin";
        settings.packet_identifiers["p2"] = "test-packet-heavy.bin";

        // Fill the orchestrator
        // TODO: later
        ochestrator.settings_identifier = "my_settings";
    }

    Interpreter interpreter;

    ParseResult result = interpreter.parse_script(script_file);

    EXPECT_DSL_EQ(correct_data, interpreter.script_);

    EXPECT_TRUE(result.success) << "Parsing failed: "
                                << result.reason;

}
