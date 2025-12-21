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
