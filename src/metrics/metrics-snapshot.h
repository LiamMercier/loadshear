#pragma once

struct MetricsSnapshot
{
    uint64_t bytes_sent{0};
    uint64_t bytes_read{0};

    uint64_t connection_attempts{0};
    uint64_t failed_connections{0};
    uint64_t finished_connections{0};

    std::array<uint64_t, 16> connection_latency_buckets{};
};
