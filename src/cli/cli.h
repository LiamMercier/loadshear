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

    // TODO <feature>: sigint() or interrupt() for handling ctrl-c from user.

private:
    int execute_script(const DSLData & script);

    template <typename Session>
    int start_orchestrator_loop(ExecutionPlan<Session> plan);

    template <typename Session>
    int start_orchestrator_loop_uninteractive(ExecutionPlan<Session> plan);

    template <typename Session>
    void dry_run(const ExecutionPlan<Session> & plan,
                 const DSLData & data);

    bool request_acknowledgement(std::string endpoints_list);

    void metric_sink(MetricsAggregate data);

private:
    const CLIOptions cli_ops_;

    std::pmr::monotonic_buffer_resource arena_;
};
