// Copyright (c) 2026 Liam Mercier
//
// This file is part of Loadshear.
//
// Loadshear is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License Version 3.0
// as published by the Free Software Foundation.
//
// Loadshear is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License v3.0
// for more details.
//
// You should have received a copy of the GNU General Public License v3.0
// along with Loadshear. If not, see <https://www.gnu.org/licenses/gpl-3.0.txt>

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
    // Helpers to make actions.
    inline void make_create(uint32_t start,
                            uint32_t end,
                            uint32_t offset_ms);

    inline void make_connect(uint32_t start,
                             uint32_t end,
                             uint32_t offset_ms);

    inline void make_send(uint32_t start,
                          uint32_t end,
                          uint32_t send_count,
                          uint32_t offset_ms);

    inline void make_flood(uint32_t start,
                           uint32_t end,
                           uint32_t offset_ms);

    inline void make_drain(uint32_t start,
                           uint32_t end,
                           uint32_t timeout,
                           uint32_t offset_ms);

    inline void make_disconnect(uint32_t start,
                                uint32_t end,
                                uint32_t offset_ms);

    std::string type_to_string() const
    {
        switch (type)
        {
            case ActionType::CREATE:
            {
                return "CREATE";
            }
            case ActionType::CONNECT:
            {
                return "CONNECT";
            }
            case ActionType::SEND:
            {
                return "SEND";
            }
            case ActionType::FLOOD:
            {
                return "FLOOD";
            }
            case ActionType::DRAIN:
            {
                return "DRAIN";
            }
            case ActionType::DISCONNECT:
            {
                return "DISCONNECT";
            }
            default:
            {
                return "ERROR";
            }
        }
    }

    // Data members.
    ActionType type;

    // Session range for the action, range is [sessions_start, sessions_end)
    uint32_t sessions_start;
    uint32_t sessions_end;

    // For SEND to specify copies.
    uint32_t count;

    // Record the timepoint offset for our orchestrator's timer loop.
    std::chrono::milliseconds offset;
};

inline void ActionDescriptor::make_create(uint32_t start,
                                          uint32_t end,
                                          uint32_t offset_ms)
{
    type = ActionType::CREATE;

    sessions_start = start;
    sessions_end = end;
    count = end - start;
    offset = std::chrono::milliseconds(offset_ms);
}

inline void ActionDescriptor::make_connect(uint32_t start,
                                           uint32_t end,
                                           uint32_t offset_ms)
{
    type = ActionType::CONNECT;

    sessions_start = start;
    sessions_end = end;
    offset = std::chrono::milliseconds(offset_ms);
}

inline void ActionDescriptor::make_send(uint32_t start,
                                        uint32_t end,
                                        uint32_t send_count,
                                        uint32_t offset_ms)
{
    type = ActionType::SEND;

    sessions_start = start;
    sessions_end = end;
    count = send_count;
    offset = std::chrono::milliseconds(offset_ms);
}

inline void ActionDescriptor::make_flood(uint32_t start,
                                         uint32_t end,
                                         uint32_t offset_ms)
{
    type = ActionType::FLOOD;

    sessions_start = start;
    sessions_end = end;
    offset = std::chrono::milliseconds(offset_ms);
}

inline void ActionDescriptor::make_drain(uint32_t start,
                                         uint32_t end,
                                         uint32_t timeout,
                                         uint32_t offset_ms)
{
    type = ActionType::DRAIN;

    sessions_start = start;
    sessions_end = end;
    count = timeout;
    offset = std::chrono::milliseconds(offset_ms);
}

inline void ActionDescriptor::make_disconnect(uint32_t start,
                                              uint32_t end,
                                              uint32_t offset_ms)
{
    type = ActionType::DISCONNECT;

    sessions_start = start;
    sessions_end = end;
    offset = std::chrono::milliseconds(offset_ms);
}
