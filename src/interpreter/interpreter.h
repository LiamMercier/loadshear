// Copyright (c) 2026 Liam Mercier
//
// This file is part of Loadshear.
//
// Loadshear is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License Version 3.0
// as published by the Free Software Foundation.
//
// Loadshear is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License v3.0
// for more details.
//
// You should have received a copy of the GNU General Public License v3.0
// along with Loadshear. If not, see <https://www.gnu.org/licenses/gpl-3.0.txt>

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
