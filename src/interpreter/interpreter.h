#pragma once

#include <filesystem>
#include <vector>

#include "parse-result.h"
#include "token.h"

class Interpreter
{
public:
    ParseResult parse_script(std::filesystem::path script_path);
private:
    std::vector<Token> tokens_;
};
