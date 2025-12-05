#pragma once

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

    // Session range for the action
    uint32_t sessions_start;
    uint32_t sessions_end;

    // For SEND or CREATE to specify copies.
    //
    // For DRAIN, this defines the seconds until we force stop.
    uint32_t count;
};
