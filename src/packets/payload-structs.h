#pragma once

#include <vector>
#include <atomic>
#include <span>
#include <new>

#include <boost/asio.hpp>

static constexpr auto COUNTER_ALIGNMENT = std::hardware_destructive_interference_size;

// Align with hardware_destructive_interference_size to prevent cache line invalidation
// across our different shard threads calling the counter.
struct alignas(COUNTER_ALIGNMENT) PayloadCounter
{
    std::atomic<uint64_t> counter;
    uint16_t step;
};

enum class PacketOperationType : uint8_t
{
    IDENTITY,
    COUNTER,
    TIMESTAMP
};

enum class TimestampFormat : uint8_t
{
    SECONDS,
    MILLISECONDS,
    MICROSECONDS,
    NANOSECONDS
};

// TODO: make create_operation_type
struct PacketOperation
{
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
