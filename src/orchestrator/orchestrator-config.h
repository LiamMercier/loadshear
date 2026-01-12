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

#include <vector>

#include "session-config.h"
#include "host-info.h"
#include "shard.h"

template<typename Session>
struct OrchestratorConfig
{
    OrchestratorConfig(SessionConfig s_conf,
                       HostInfo<Session> h_info,
                       Shard<Session>::MessageHandlerFactory msg_factory,
                       size_t num_shards)
    :session_config(std::move(s_conf)),
    host_info(std::move(h_info)),
    handler_factory_(std::move(msg_factory)),
    shard_count(num_shards)    
    {
    }

    SessionConfig session_config;
    HostInfo<Session> host_info;
    Shard<Session>::MessageHandlerFactory handler_factory_;
    size_t shard_count;
};
