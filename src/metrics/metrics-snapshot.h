#pragma once

struct MetricsSnapshot
{
    uint64_t bytes_sent{0};
    uint64_t bytes_read{0};
    std::array<uint64_t, 16> connection_latency_buckets{};
};
