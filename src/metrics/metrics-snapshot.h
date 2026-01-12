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

#pragma once

#include <vector>
#include <chrono>

#ifdef __cpp_lib_hardware_interference_size
static constexpr size_t CACHE_ALIGNMENT = std::hardware_destructive_interference_size;
#else
static constexpr size_t CACHE_ALIGNMENT = 64;
#endif

// A snapshot of data from the shard (or aggregate of all shard data).
struct MetricsSnapshot
{
public:
    static constexpr size_t NUM_BUCKETS = 16;

public:
    MetricsSnapshot & operator+=(const MetricsSnapshot & rhs)
    {
        bytes_sent += rhs.bytes_sent;
        bytes_read += rhs.bytes_read;

        connection_attempts += rhs.connection_attempts;
        failed_connections += rhs.failed_connections;
        finished_connections += rhs.finished_connections;

        connected_sessions += rhs.connected_sessions;

        for (size_t i = 0; i < NUM_BUCKETS; i++)
        {
            connection_latency_buckets[i] += rhs.connection_latency_buckets[i];
            send_latency_buckets[i] += rhs.send_latency_buckets[i];
            read_latency_buckets[i] += rhs.read_latency_buckets[i];
        }

        return *this;
    }

    uint64_t bytes_sent{0};
    uint64_t bytes_read{0};

    uint64_t connection_attempts{0};
    uint64_t failed_connections{0};
    uint64_t finished_connections{0};

    // This must be filled by the shards during the request
    // since our ShardMetrics object does not have access to the
    // SessionPool to grab the data.
    uint64_t connected_sessions{0};

    std::array<uint64_t, NUM_BUCKETS> connection_latency_buckets{};
    std::array<uint64_t, NUM_BUCKETS> send_latency_buckets{};
    std::array<uint64_t, NUM_BUCKETS> read_latency_buckets{};
};

// Signed version of MetricsSnapshot, most fields should never be negative
// but this does prevent overflow from program logic errors.
struct MetricsDelta
{
public:
    static constexpr size_t NUM_BUCKETS = MetricsSnapshot::NUM_BUCKETS;

public:
    inline void compute_difference(const MetricsSnapshot & current,
                                   const MetricsSnapshot & previous);

    int64_t bytes_sent{0};
    int64_t bytes_read{0};

    int64_t connection_attempts{0};
    int64_t failed_connections{0};
    int64_t finished_connections{0};

    // It's likely this will be negative when winding down.
    int64_t connected_sessions{0};

    std::array<int64_t, NUM_BUCKETS> connection_latency_buckets{};
    std::array<int64_t, NUM_BUCKETS> send_latency_buckets{};
    std::array<int64_t, NUM_BUCKETS> read_latency_buckets{};
};

inline void MetricsDelta::compute_difference(const MetricsSnapshot & current,
                                             const MetricsSnapshot & previous)
{
    // Calculate the change between snapshots.
    bytes_sent = static_cast<int64_t>(current.bytes_sent)
                 - static_cast<int64_t>(previous.bytes_sent);

    bytes_read = static_cast<int64_t>(current.bytes_read)
                 - static_cast<int64_t>(previous.bytes_read);

    connection_attempts = static_cast<int64_t>(current.connection_attempts)
                          - static_cast<int64_t>(previous.connection_attempts);

    failed_connections = static_cast<int64_t>(current.failed_connections)
                         - static_cast<int64_t>(previous.failed_connections);

    finished_connections = static_cast<int64_t>(current.finished_connections)
                           - static_cast<int64_t>(previous.finished_connections);

    connected_sessions = static_cast<int64_t>(current.connected_sessions)
                         - static_cast<int64_t>(previous.connected_sessions);

    for (size_t i = 0; i < NUM_BUCKETS; i++)
    {
        connection_latency_buckets[i] = static_cast<int64_t>
                                            (
                                                current.connection_latency_buckets[i]
                                            )
                                        - static_cast<int64_t>
                                            (
                                                previous.connection_latency_buckets[i]
                                            );

        send_latency_buckets[i] = static_cast<int64_t>
                                            (
                                                current.send_latency_buckets[i]
                                            )
                                        - static_cast<int64_t>
                                            (
                                                previous.send_latency_buckets[i]
                                            );

        read_latency_buckets[i] = static_cast<int64_t>
                                            (
                                                current.read_latency_buckets[i]
                                            )
                                        - static_cast<int64_t>
                                            (
                                                previous.read_latency_buckets[i]
                                            );
    }
}

// Hold the current snapshot and change from the last snapshot.
struct MetricsAggregate
{
    // Newest metric snapshot aggregated across all shards.
    MetricsSnapshot current_snapshot_aggregate;

    // Difference between this snapshot and the last.
    MetricsDelta change_aggregate;

    // Time offset from startup.
    std::chrono::steady_clock::duration offset;
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
