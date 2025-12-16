#pragma once

#include <optional>
#include <string>

struct CLIOptions
{
    std::string script_file;
    bool dry_run;
    bool expand_envs;
};

std::optional<CLIOptions> parse_cli(int argc, char** argv);
