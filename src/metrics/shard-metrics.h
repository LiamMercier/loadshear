#pragma once

#include <cstdint>
#include <cstddef>
#include <array>

#include "metrics-snapshot.h"

// Handle alignment for thread interference.
//
// We absolutely do not want any way for ShardMetrics for shard A and B to
// get placed beside each other in memory. Rare, but possible if we make future changes.
struct alignas(CACHE_ALIGNMENT) ShardMetrics
{
public:
    static constexpr size_t NUM_BUCKETS = MetricsSnapshot::NUM_BUCKETS;

public:
    void record_connection_latency(uint64_t latency_us);

    void record_send_latency(uint64_t latency_us);

    void record_read_latency(uint64_t latency_us);

    inline void record_bytes_sent(uint64_t count);

    inline void record_bytes_read(uint64_t count);

    inline void record_connection_attempt();

    inline void record_connection_fail();

    inline void record_connection_success();

    MetricsSnapshot fetch_snapshot();

private:
    uint64_t bytes_sent{0};
    uint64_t bytes_read{0};

    uint64_t connection_attempts{0};
    uint64_t failed_connections{0};
    uint64_t finished_connections{0};

    // We map time values to buckets based on log multiples of 64us.
    //
    // 0 : < 64us
    // 1 : < 128us
    // 2 : < 256us
    // 3 : < 512us
    // .
    // .
    // .
    // 14 : ~1s
    // 15 : ~2s
    std::array<uint64_t, NUM_BUCKETS> connection_latency_buckets{};
    std::array<uint64_t, NUM_BUCKETS> send_latency_buckets{};
    std::array<uint64_t, NUM_BUCKETS> read_latency_buckets{};
};

inline void ShardMetrics::record_bytes_sent(uint64_t count)
{
    bytes_sent += count;
}

inline void ShardMetrics::record_bytes_read(uint64_t count)
{
    bytes_read += count;
}

inline void ShardMetrics::record_connection_attempt()
{
    connection_attempts += 1;
}

inline void ShardMetrics::record_connection_fail()
{
    failed_connections += 1;
}

inline void ShardMetrics::record_connection_success()
{
    finished_connections += 1;
}
