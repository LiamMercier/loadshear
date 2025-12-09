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

public:
    DSLData script_;

private:
    std::vector<Token> tokens_;
};
