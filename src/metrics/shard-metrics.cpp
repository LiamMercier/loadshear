#include "shard-metrics.h"

void ShardMetrics::record_connection_latency(uint64_t latency_us)
{
    // Clamp to 64us, anything less is basically impossible to measure in our case.
    if (latency_us < 64)
    {
        connection_latency_buckets[0] += 1;
        return;
    }

    // Quickly compute branchless log2 using bit width.
    unsigned int bucket = std::bit_width(latency_us);

    // 64us = 2^6 so remove the offset.
    int index = bucket - 6;

    // Handle overflow (very long time).
    if (index >= 15)
    {
        index = 15;
    }

    connection_latency_buckets[index] += 1;
}

MetricsSnapshot ShardMetrics::fetch_snapshot()
{
    MetricsSnapshot res;

    res.bytes_sent = bytes_sent;
    res.bytes_read = bytes_read;

    res.connection_attempts = connection_attempts;
    res.failed_connections = failed_connections;
    res.finished_connections = finished_connections;

    res.connection_latency_buckets = connection_latency_buckets;

    return res;
}
