#pragma once

enum class ActionType : uint8_t
{
    CREATE = 0,
    CONNECT,
    SEND,
    FLOOD,
    DISCONNECT
};

struct ActionDescriptor
{
    ActionType type;

    // Session range for the action
    uint32_t sessions_start;
    uint32_t sessions_end;

    // For SEND or CREATE to specify copies.
    uint32_t count;

    // TODO: decide if we really need this.
    uint32_t host_id;
};
