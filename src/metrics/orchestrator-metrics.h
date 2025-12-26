#pragma once

#include "shard-metrics.h"

// Handle alignment for thread interference.
//
// We absolutely do not want thread A and thread B to touch
// the same cache line while writing to the orchestrator
#ifdef __cpp_lib_hardware_interference_size
static constexpr size_t ALIGNMENT = std::hardware_destructive_interference_size;
#else
static constexpr size_t ALIGNMENT = 64;
#endif

// TODO: decide on write method.
struct alignas(ALIGNMENT) OrchestratorMetrics
{
    // Hold a history of metric snapshots for each shard.
    std::vector<std::vector<MetricsSnapshot>> shard_metric_history;
}

// TODO: preallocate vectors based on last offset time.
