#pragma once

#include <vector>
#include <string>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "action-descriptor.h"

template<typename T>
inline constexpr bool always_false{false};

// TODO <feature>: Update this when more transports are available.
const std::unordered_set<std::string> VALID_PROTOCOLS {
    "TCP"
};

// Does not include user defined .wasm files.
const std::unordered_set<std::string> VALID_MESSAGE_HANDLERS {
    "NOP"
};

const std::unordered_set<std::string> VALID_TIME_FORMATS
{
    "seconds",
    "milliseconds",
    "microseconds",
    "nanoseconds",
};

struct Range
{
    uint32_t start;
    // For Action, this is the end, for modifications, this is length.
    uint32_t second;

    // If we store start:length then this computes the end.
    uint32_t end_from_length() const
    {
        // start + length
        return start + second;
    }

    uint32_t size_from_end() const
    {
        // end - start
        return second - start;
    }

    bool operator==(const Range & other) const
    {
        return ((start == other.start) && (second == other.second));
    }
};

enum class ModificationType : uint8_t
{
    Counter,
    Timestamp
};

struct CounterModification
{
    // start:length
    Range counter_bytes;

    uint32_t counter_step;
    bool little_endian;

    bool operator==(const CounterModification & other) const = default;
};

struct TimestampModification
{
    Range timestamp_bytes;

    // For options like "little":"seconds"
    bool little_endian;
    std::string format_name;

    bool operator==(const TimestampModification & other) const = default;
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

    // For COUNTER or TIMESTAMP operations.
    std::vector<TimestampModification> timestamp_mods;
    std::vector<CounterModification> counter_mods;

    // Keep track of the order we pushed these in.
    std::vector<ModificationType> mod_order;

    // To push back a modifier
    template <typename T>
    void push_modifier(T & mod)
    {
        // Push back TimestampModification
        if constexpr (std::is_same_v<T, TimestampModification>)
        {
            timestamp_mods.push_back(std::move(mod));
            mod_order.push_back(ModificationType::Timestamp);
        }
        // CounterModification
        else if constexpr (std::is_same_v<T, CounterModification>)
        {
            counter_mods.push_back(std::move(mod));
            mod_order.push_back(ModificationType::Counter);
        }
        // Bad usage.
        else
        {
            static_assert(always_false<T>,
                          "push_modifier() not implemented for this type");
        }
    }

};

struct SettingsBlock
{
    std::string identifier;
    std::string session_protocol;

    uint32_t header_size;
    uint32_t body_max;
    bool read{false};
    bool repeat{false};

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
