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
