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
    UDP,
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

    std::string dump_endpoint_list() const;

public:

    // Actions to feed into the orchestrator.
    std::vector<ActionDescriptor> actions;

    // Payload descriptors for each payload.
    std::vector<PayloadDescriptor> payloads;
    std::vector<std::vector<uint16_t>> counter_steps;

    // Settings for the orchestrator ripped from the script.
    OrchestratorConfig<Session> config;

    // Arena allocated packet buffers.
    std::pmr::vector<std::pmr::vector<uint8_t>> packet_data;
};

template<typename Session>
std::expected<ExecutionPlan<Session>, std::string>
generate_execution_plan(const DSLData & script,
                        std::pmr::memory_resource* memory);
