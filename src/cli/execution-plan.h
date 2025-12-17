#pragma once

#include "script-structs.h"

#include "action-descriptor.h"
#include "orchestrator-config.h"
#include "payload-structs.h"

#include <expected>
#include <memory_resource>

enum class ProtocolType : uint8_t
{
    TCP,
    UNDEFINED
};

template<typename Session>
struct ExecutionPlan
{
    ExecutionPlan(OrchestratorConfig<Session> o_config,
                  std::pmr::vector<std::pmr::vector<uint8_t>> data)
    :config(std::move(o_config)),
    packet_data(std::move(data))
    {
    }

    // Actions to feed into the orchestrator.
    std::vector<ActionDescriptor> actions;

    // Payload descriptors for each payload.
    std::vector<PayloadDescriptor> payloads;
    std::vector<uint16_t> counter_steps;

    OrchestratorConfig<Session> config;

    // Arena allocated packet buffers.
    std::pmr::vector<std::pmr::vector<uint8_t>> packet_data;
};

template<typename Session>
std::expected<ExecutionPlan<Session>, std::string>
generate_execution_plan(const DSLData & script,
                        std::pmr::memory_resource* memory);
