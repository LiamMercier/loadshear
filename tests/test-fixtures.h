#pragma once

#include "script-structs.h"
#include "execution-plan.h"
#include "all-transports.h"

DSLData get_simple_valid_script_data();

ExecutionPlan<TCPSession>
get_simple_valid_script_plan(std::pmr::memory_resource* memory);
