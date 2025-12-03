#pragma once

enum class ActionType : uint8_t
{
    CONNECT = 0,
    SEND
};

struct ActionDescriptor
{
    ActionType type;

    // Session range for the action
    uint32_t sessions_start;
    uint32_t sessions_end;

    // For SEND to specify copies.
    uint32_t copies;

    // TODO: decide if we really need this.
    uint32_t host_id;
};
