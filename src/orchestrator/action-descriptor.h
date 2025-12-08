#pragma once

#include <chrono>

enum class ActionType : uint8_t
{
    CREATE = 0,
    CONNECT,
    SEND,
    FLOOD,
    DRAIN,
    DISCONNECT
};

struct ActionDescriptor
{
    ActionType type;

    // Session range for the action, range is [sessions_start, sessions_end)
    uint32_t sessions_start;
    uint32_t sessions_end;

    // For SEND to specify copies.
    //
    // For DRAIN, this defines the seconds until we force stop.
    uint32_t count;

    // Record the timepoint offset for our orchestrator's timer loop.
    std::chrono::milliseconds offset;
};
