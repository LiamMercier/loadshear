#pragma once

#include <vector>

#ifdef __cpp_lib_hardware_interference_size
static constexpr size_t CACHE_ALIGNMENT = std::hardware_destructive_interference_size;
#else
static constexpr size_t CACHE_ALIGNMENT = 64;
#endif

struct MetricsSnapshot
{
    uint64_t bytes_sent{0};
    uint64_t bytes_read{0};

    uint64_t connection_attempts{0};
    uint64_t failed_connections{0};
    uint64_t finished_connections{0};

    // This must be filled by the shard during the request
    // since our ShardMetrics object does not have access to the
    // SessionPool to grab the data.
    uint64_t connected_sessions{0};

    std::array<uint64_t, 16> connection_latency_buckets{};
};

// Each list reference is aligned so we never do false sharing when shards
// are writing back to the orchestrator.
struct alignas(CACHE_ALIGNMENT) SnapshotList
{
    void push_back(const MetricsSnapshot & snapshot)
    {
        snapshots.push_back(snapshot);
    }

    void push_back(MetricsSnapshot && snapshot)
    {
        snapshots.push_back(std::move(snapshot));
    }

    std::vector<MetricsSnapshot> snapshots;
};
