#pragma once

#include "script-structs.h"

#include "action-descriptor.h"
#include "orchestrator-config.h"
#include "payload-structs.h"

enum class ProtocolType : uint8_t
{
    TCP,
    UNDEFINED
};

template<typename Session>
struct ExecutionPlan
{
    // Actions to feed into the orchestrator.
    std::vector<ActionDescriptor> actions;

    // Payload descriptors for each payload.
    std::vector<PayloadDescriptor> payloads;
    std::vector<uint16_t> counter_steps;

    OrchestratorConfig<Session> config;
};

template<typename Session>
ExecutionPlan<Session> generate_execution_plan(const DSLData & script);
