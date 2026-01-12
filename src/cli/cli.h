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
