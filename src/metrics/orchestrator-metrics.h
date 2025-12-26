#pragma once

#include "shard-metrics.h"

struct OrchestratorMetrics
{
    // Hold a history of metric snapshots for each shard.
    std::vector<SnapshotList> shard_metric_history;
}

// TODO: preallocate vectors based on last offset time.
