#pragma once

#include "shard-metrics.h"

struct OrchestratorMetrics
{
    void reserve_lists(size_t num_metrics)
    {
        for (auto & list : shard_metric_history)
        {
            list.snapshots.reserve(num_metrics);
        }
    }

    // Hold a history of metric snapshots for each shard.
    std::vector<SnapshotList> shard_metric_history;
};
