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

#include <string>
#include <cstdint>

struct CLIOptions
{
    std::string script_file;
    bool dry_run{false};
    bool expand_envs{false};
    bool acknowledged_responsibility{false};
    bool quiet{false};
    uint64_t arena_init_mb{0};
};

struct CLIParseResult
{
    enum class ParseStatus
    {
        Ok = 0,
        Help,
        Version,
        Error
    };

    CLIOptions options;
    ParseStatus status;

    bool good_parse()
    {
        return (status == ParseStatus::Ok);
    }

    // If the operation exited, why?
    int status_code()
    {
        switch (status)
        {
            case ParseStatus::Error:
            {
                return 1;
            }
            default:
            {
                return 0;
            }
        }
    }
};

CLIParseResult parse_cli(int argc, char** argv);
