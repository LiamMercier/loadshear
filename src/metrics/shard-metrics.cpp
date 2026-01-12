// Copyright (c) 2026 Liam Mercier
//
// This file is part of Loadshear.
//
// Loadshear is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License Version 3.0
// as published by the Free Software Foundation.
//
// Loadshear is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License v3.0
// for more details.
//
// You should have received a copy of the GNU General Public License v3.0
// along with Loadshear. If not, see <https://www.gnu.org/licenses/gpl-3.0.txt>

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

void ShardMetrics::record_send_latency(uint64_t latency_us)
{
    // Clamp to 64us, anything less is basically impossible to measure in our case.
    if (latency_us < 64)
    {
        send_latency_buckets[0] += 1;
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

    send_latency_buckets[index] += 1;
}

void ShardMetrics::record_read_latency(uint64_t latency_us)
{
    // Clamp to 64us, anything less is basically impossible to measure in our case.
    if (latency_us < 64)
    {
        read_latency_buckets[0] += 1;
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

    read_latency_buckets[index] += 1;
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
    res.send_latency_buckets = send_latency_buckets;
    res.read_latency_buckets = read_latency_buckets;

    return res;
}
