#pragma once

#include <vector>

#include "session-config.h"
#include "host-info.h"

template<typename Session>
struct OrchestratorConfig
{
    std::vector<uint8_t> wasm_module_bytes
    SessionConfig session_config;
    HostInfo<Session> host_info;
    wasmtime::Config wasm_config;
    size_t shard_count;
};
