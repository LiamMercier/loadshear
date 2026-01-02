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

    inline MetricsAggregate get_aggregate_delta() const;

    // Hold a history of metric snapshots for each shard.
    std::vector<SnapshotList> shard_metric_history;
};

inline MetricsAggregate OrchestratorMetrics::get_aggregate_delta() const
{
    MetricsAggregate agg;

    // Compute the aggregate of the last and second last metrics, subtract.
    MetricsSnapshot current;
    MetricsSnapshot prev;

    for (const auto & ss_list : shard_metric_history)
    {
        size_t N = ss_list.snapshots.size();

        // For safety, should never really happen.
        if (N == 0)
        {
            continue;
        }

        const auto & latest_ss = ss_list.snapshots[N - 1];
        current += latest_ss;

        if (N >= 2)
        {
            const auto & prev_ss = ss_list.snapshots[N - 2];
            prev += prev_ss;
        }
    }

    agg.change_aggregate.compute_difference(current, prev);
    agg.current_snapshot_aggregate = std::move(current);

    return agg;
}
