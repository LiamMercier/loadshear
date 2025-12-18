#pragma once

#include <memory_resource>

#include "cli-parsing.h"
#include "script-structs.h"
#include "execution-plan.h"

class CLI
{
public:
    explicit CLI(CLIOptions ops);

    int run();

private:
    int execute_script(const DSLData & script);

    template <typename Session>
    int start_orchestrator_loop(ExecutionPlan<Session> plan);

    template <typename Session>
    void dry_run(const ExecutionPlan<Session> & plan);

private:
    const CLIOptions cli_ops_;

    std::pmr::monotonic_buffer_resource arena_;
};
