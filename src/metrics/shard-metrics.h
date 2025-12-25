#pragma once

#include <cstdint>
#include <cstddef>
#include <array>

// Handle alignment for thread interference.
//
// We absolutely do not want any way for ShardMetrics for shard A and B to
// get placed beside each other in memory. Rare, but possible if we make future changes.
#ifdef __cpp_lib_hardware_interference_size
static constexpr size_t ALIGNMENT = std::hardware_destructive_interference_size;
#else
static constexpr size_t ALIGNMENT = 64;
#endif

struct alignas(ALIGNMENT) ShardMetrics
{
public:
    void record_connection_latency(uint64_t latency_us);

public:
    uint64_t bytes_sent{0};
    uint64_t bytes_read{0};

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
    std::array<uint64_t, 16> connection_latency_buckets{};
};

