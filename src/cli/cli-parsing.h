#pragma once

#include <string>

struct CLIOptions
{
    std::string script_file;
    bool dry_run;
    bool expand_envs;
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
