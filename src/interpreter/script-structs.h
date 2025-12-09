#pragma once

#include <vector>
#include <string>
#include <optional>
#include <unordered_map>

#include "action-descriptor.h"

struct Range
{
    uint32_t start;
    uint32_t end;
};

struct Action
{
    ActionType type;

    // Common for all actions.
    uint32_t count;
    Range range;
    uint32_t offset_ms;

    //
    // SEND specific.
    //
    std::string packet_identifier;

    // Hold start:length for the insertion.
    std::optional<Range> timestamp_range;
    std::optional<Range> counter_range;

    // Hold the counter step, if little endian, name of timestamp format.
    uint32_t counter_step;
    bool little_endian;
    std::vector<std::string> format_name;

    //
    // DRAIN specific
    //
    uint32_t timeout_ms;
};

struct SettingsBlock
{
    std::string identifier;
    std::string session_protocol;

    uint32_t header_size;
    uint32_t body_max;
    bool read;
    bool repeat;

    uint32_t shards;

    std::string handler_value;
    std::vector<std::string> endpoints;
    std::unordered_map<std::string, std::string> packet_identifiers;
};

struct OrchestratorBlock
{
    std::string settings_identifier;
    std::vector<Action> actions;
};

// For now, just assume one settings and one orchestrator.
struct DSLData
{
    SettingsBlock settings;
    OrchestratorBlock orchestrator;
};
