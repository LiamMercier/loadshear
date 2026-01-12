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
#include <atomic>
#include <span>
#include <new>

#include <boost/asio.hpp>

// Handle case where we don't have the cache line size and set it to 64.
#ifdef __cpp_lib_hardware_interference_size
static constexpr size_t COUNTER_ALIGNMENT = std::hardware_destructive_interference_size;
#else
static constexpr size_t COUNTER_ALIGNMENT = 64;
#endif

// Align with hardware_destructive_interference_size to prevent cache line invalidation
// across our different shard threads calling the counter.
struct alignas(COUNTER_ALIGNMENT) PayloadCounter
{
public:
    // Methods to make the payload counter viable in vectors.
    //
    // We follow the rule of 5 by having moves be copies.

    PayloadCounter() noexcept
    :counter(0),
    step(0)
    {
    }

    // Copy construct
    PayloadCounter(const PayloadCounter & rhs) noexcept
    :counter(rhs.counter.load(std::memory_order_relaxed)),
    step(rhs.step)
    {
    }

    // Copy, likely unused for most of our uses.
    PayloadCounter & operator=(const PayloadCounter & rhs) noexcept
    {
        counter.store(rhs.counter.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
        step = rhs.step;
        return *this;
    }

    // Implement moves as copies, since we can't really move the atomic.
    PayloadCounter(PayloadCounter && rhs) noexcept
    :counter(rhs.counter.load(std::memory_order_relaxed)),
    step(rhs.step)
    {
    }

    PayloadCounter & operator=(PayloadCounter && rhs) noexcept
    {
        counter.store(rhs.counter.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
        step = rhs.step;
        return *this;
    }

public:
    std::atomic<uint64_t> counter{0};
    uint16_t step;
};

// Operations allowed for the user.
enum class PacketOperationType : uint8_t
{
    IDENTITY = 0,
    COUNTER,
    TIMESTAMP
};

enum class TimestampFormat : uint8_t
{
    Seconds = 0,
    Milliseconds,
    Microseconds,
    Nanoseconds
};

struct PacketOperation
{
    void make_identity(uint32_t len)
    {
        type = PacketOperationType::IDENTITY;
        length = len;

        // Irrelevant data, just init to 0.
        little_endian = 0;
        time_format = TimestampFormat::Seconds;
    }

    void make_counter(uint32_t len, bool little_end)
    {
        type = PacketOperationType::COUNTER;
        length = len;
        little_endian = little_end;

        // Irrelevant, just init to 0.
        time_format = TimestampFormat::Seconds;
    }

    void make_timestamp(uint32_t len, bool little_end, TimestampFormat format)
    {
        type = PacketOperationType::TIMESTAMP;
        length = len;
        little_endian = little_end;
        time_format = format;
    }

    PacketOperationType type;

    // Insert length, platform type.
    uint32_t length;
    bool little_endian;
    TimestampFormat time_format;

    // Constants used during initial program parsing.
    static constexpr uint32_t MAX_LENGTH = UINT32_MAX;
    static constexpr uint16_t MAX_STEP_SIZE = UINT16_MAX;
    static constexpr size_t MAX_COUNTER_LENGTH = sizeof(uint64_t);
    static constexpr size_t MAX_TIMESTAMP_LENGTH = sizeof(uint64_t);
};

// Each PayloadDescriptor contains:
// - A pointer to the raw packet data (after any inline computation during startup).
// - A list of per Session operations to apply to the data.
struct PayloadDescriptor
{
    // Packet will always exist as long as a Session is running, since we assume
    // that the shard does not shutdown until every Session is closed.
    std::span<const uint8_t> packet_data;
    std::vector<PacketOperation> ops;
};

// The goal of the prepared payload is to make scatter-gather IO easy for the calling Session.
//
// We assume that the prepared payload is the static payload with some (usually small) portions
// cut out of it.
struct PreparedPayload
{
    PreparedPayload() = default;

    PreparedPayload(PreparedPayload &&) = default;
    PreparedPayload& operator=(PreparedPayload&&) = default;

    // Prevent copies.
    PreparedPayload(const PreparedPayload &) = delete;
    PreparedPayload & operator=(const PreparedPayload &) = delete;

    // Keep capacity but setup vector as if we just called reserve
    void clear()
    {
        temps.clear();
        packet_slices.clear();
    }

    // Dynamic bytes we inserted (counters, timestamps).
    // packet_slices holds a reference to these along with its static data.
    //
    // Inserts into temps may reallocate and invalidate the packet_slices inserted, so
    // we MUST reserve enough space ahead of time.
    std::vector<uint8_t> temps;

    // Stores the read only slices of the base packet and the slices in temps.
    std::vector<boost::asio::const_buffer> packet_slices;
};
