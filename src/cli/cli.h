#pragma once

#include "cli-parsing.h"

class CLI
{
public:
    explicit CLI(CLIOptions ops);

    int run();

private:
    const CLIOptions cli_ops_;
};
