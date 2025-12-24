#pragma once

#include <cstdint>
#include <array>
#include <atomic>

struct ShardMetrics
{
public:
    void record_latency(uint64_t latency_us);

public:
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
    std::array<std::atomic<uint64_t>, 16> latency_buckets;
};

