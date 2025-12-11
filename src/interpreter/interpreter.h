#pragma once

#include <filesystem>
#include <vector>

#include "parse-result.h"
#include "token.h"
#include "script-structs.h"

class Interpreter
{
public:
    ParseResult parse_script(std::filesystem::path script_path);

    void set_script_defaults();

    ParseResult verify_script();

private:
    ParseResult good_parse();

public:
    DSLData script_;

private:
    std::vector<Token> tokens_;
};
