#pragma once

#include <memory_resource>

#include "cli-parsing.h"
#include "script-structs.h"

class CLI
{
public:
    explicit CLI(CLIOptions ops);

    int run();

    int execute_script(const DSLData & script);

private:
    const CLIOptions cli_ops_;

    std::pmr::monotonic_buffer_resource arena_;
};
