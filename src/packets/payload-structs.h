#pragma once

#include <vector>

struct PayloadCounter
{
    size_t payload_index;
    std::atomic<size_t> counter;
};

enum class PacketOperationType : uint8_t
{
    IDENTITY,
    COUNTER,
    TIMESTAMP
};

struct PacketOperation
{
    PacketOperationType type;

    // Where to start insert, the length, platform type.
    uint32_t offset;
    uint16_t length;
    bool little_endian;

    // Constants used during initial program parsing.
    static constexpr uint32_t MAX_OFFSET = UINT32_MAX;
    static constexpr uint16_t MAX_LENGTH = UINT16_MAX;
};

// Each PayloadDescriptor contains:
// - A pointer to the raw packet data (after any inline computation during startup).
// - A list of per Session operations to apply to the data.
struct PayloadDescriptor
{
    // Packet will always exist as long as a Session is running.
    std::span<const uint8_t> packet_data;
    std::vector<PacketOperation> ops;
};

// TODO: decide concretely on how this should actually work.
struct PreparedPayload
{
    PreparedPayload() = default;

    PreparedPayload(PreparedPayload &&) = default;
    PreparedPayload& operator=(PreparedPayload&&) = default;

    // Prevent copies.
    PreparedPayload(const PreparedPayload) = delete;
    PreparedPayload & operator=(const PreparedPayload &) = delete;

    // Dynamic bytes we inserted (counters, timestamps).
    // packet_slices holds a reference to these along with its static data.
    std::vector<std::vector<uint8_t>> temps;

    // Stores the read only slices of the base packet and the slices in temps.
    std::vector<boost::asio::const_buffer> packet_slices;
};
