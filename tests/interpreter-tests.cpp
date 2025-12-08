#include <gtest/gtest.h>

#include "interpreter.h"

TEST(InterpreterTests, SimpleValidScript)
{
    std::filesystem::path script = "tests/scripts/simple-valid-script.ldsh";

    // TODO: setup
    Interpreter interpreter;

    ParseResult result = interpreter.parse_script(script);

    EXPECT_TRUE(result.success) << "Parsing failed: "
                                << result.reason;

}
