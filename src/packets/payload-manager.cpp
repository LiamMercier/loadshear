#include "payload-manager.h"

#include <chrono>
#include <iostream>

// TODO <feature>: make it so we can have multiple counter steps per payload.
PayloadManager::PayloadManager(std::vector<PayloadDescriptor> payloads,
                               std::vector<uint16_t> steps)
:payloads_(payloads),
counters_(payloads_.size())
{
    for (size_t i = 0; i < counters_.size(); i++)
    {
        counters_[i].counter = 0;

        // Prevent OOB access.
        if (i > steps.size())
        {
            counters_[i].step = 1;
            std::cerr << "Warning! Counter steps was out of bounds!\n";
        }
        else
        {
            counters_[i].step = steps[i];
        }
    }
}

bool PayloadManager::fill_payload(size_t index, PreparedPayload & payload) const
{
    if (index >= payloads_.size())
    {
        return false;
    }

    const auto & descriptor = payloads_[index];
    const auto & ops = descriptor.ops;

    // First, call payload.clear() to reset the payload (keeps allocation).
    payload.clear();

    // Next, calculate how much capacity we need, allocate if necessary.
    size_t total_temp_bytes = 0;

    for (const auto & op : ops)
    {
        // Only count dynamic data, ignore identity data.
        if (op.type != PacketOperationType::IDENTITY)
        {
            total_temp_bytes += op.length;
        }
    }

    // We need to allocate this ahead of time to prevent pointer invalidation.
    payload.temps.reserve(total_temp_bytes);

    // For each operation, we insert data (either static or dynamic).
    payload.packet_slices.reserve(ops.size());

    // Keep track of where we are.
    size_t current_offset = 0;

    const auto & view = descriptor.packet_data;

    // Iterate over the operations to create the packet.
    for (const auto & op : ops)
    {
        switch (op.type)
        {
            // Insert the static data from the span.
            case PacketOperationType::IDENTITY:
            {
                payload.packet_slices.emplace_back(view.data() + current_offset,
                                                   op.length);
                current_offset += op.length;
                break;
            }
            // Create counter data and insert it.
            case PacketOperationType::COUNTER:
            {
                uint64_t val = counters_[index].counter.fetch_add(
                                    counters_[index].step, std::memory_order_relaxed);

                size_t write_index = payload.temps.size();

                // Resize is safe here because we already reserved enough room.
                payload.temps.resize(write_index + op.length);

                // Grab the start of where we want to insert the number.
                uint8_t *start = payload.temps.data() + write_index;

                write_numeric(start, val, op.length, op.little_endian);

                // Add to the buffer's view.
                payload.packet_slices.emplace_back(start, op.length);

                current_offset += op.length;
                break;
            }
            case PacketOperationType::TIMESTAMP:
            {
                auto now = std::chrono::system_clock::now().time_since_epoch();

                size_t ts_value = 0;

                // Interpret as an 8 byte seconds based timestamp.
                switch (op.time_format)
                {
                    case TimestampFormat::Seconds:
                    {
                        ts_value = std::chrono::duration_cast<std::chrono::seconds>(now).count();
                        break;
                    }
                    case TimestampFormat::Milliseconds:
                    {
                        ts_value = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
                        break;
                    }
                    case TimestampFormat::Microseconds:
                    {
                        ts_value = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
                        break;
                    }
                    case TimestampFormat::Nanoseconds:
                    {
                        ts_value = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }

                size_t write_index = payload.temps.size();

                // Resize is safe here because we already reserved enough room.
                payload.temps.resize(write_index + op.length);

                uint8_t *start = payload.temps.data() + write_index;

                write_numeric(start, ts_value, op.length, op.little_endian);

                // Add to the buffer's view.
                payload.packet_slices.emplace_back(start, op.length);

                current_offset += op.length;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    return true;
}

// Write numeric from little endian host to buffer.
//
// TODO <feature>: We could re-write this using compiler definitions to support big endian hosts.
void PayloadManager::write_numeric(uint8_t *start,
                                   uint64_t raw_numeric,
                                   uint32_t length,
                                   bool little_endian) const
{
    if (length == 8)
    {
        uint64_t val_64_bit = raw_numeric;

        if (!little_endian)
        {
            val_64_bit = __builtin_bswap64(val_64_bit);
        }

        std::memcpy(start, &val_64_bit, 8);
        return;
    }
    else if (length == 4)
    {
        uint32_t val_32_bit = static_cast<uint32_t>(raw_numeric);

        if (!little_endian)
        {
            val_32_bit = __builtin_bswap32(val_32_bit);
        }

        std::memcpy(start, &val_32_bit, 4);
        return;
    }

    // Write each byte.
    for (uint32_t i = 0; i < length; i++)
    {
        // Compute the shift for this byte's mask. If big endian, we have to reverse.
        int shift = little_endian ? (i * 8) : ((length - 1 - i) * 8);
        start[i] = static_cast<uint8_t>((raw_numeric >> shift) & 0xFF);
    }
}
