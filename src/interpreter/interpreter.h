#pragma once

#include <filesystem>
#include <vector>
#include <map>

#include "parse-result.h"
#include "token.h"
#include "script-structs.h"

class Interpreter
{
public:
    static constexpr uint64_t DEFAULT_PACKET_SAMPLE_RATE = 100;

public:
    ParseResult parse_script(std::string script_name);

private:

    void set_script_defaults();

    ParseResult verify_script();

    ParseResult insert_mod_range(std::map<uint32_t, uint32_t> & map,
                                 Range to_insert,
                                 size_t action_id);

    ParseResult arbitrary_error(std::string reason);

    ParseResult bad_range_error(Range overlapped,
                                Range violating_range,
                                size_t action_id);

    ParseResult good_parse();

public:
    DSLData script_;

private:
    std::vector<Token> tokens_;
};
