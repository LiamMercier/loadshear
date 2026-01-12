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

#include "payload-manager.h"

#include "logger.h"

#include <chrono>
#include <iostream>

PayloadManager::PayloadManager(std::vector<PayloadDescriptor> payloads,
                               std::vector<std::vector<uint16_t>> steps)
:payloads_(std::move(payloads)),
counters_(payloads_.size())
{
    // Should basically never happen.
    if (payloads_.size() > steps.size())
    {
        std::string e_msg = "Warning! Not enough counter lists!";
        Logger::warn(std::move(e_msg));

        steps.resize(payloads_.size());
    }

    // Go over each list of counters for each payload.
    for (size_t p_id = 0; p_id < payloads_.size(); p_id++)
    {
        auto & payload_counters_ = counters_[p_id];

        auto & step_list = steps[p_id];

        // Allocate and fill the data now.
        payload_counters_.resize(step_list.size());

        // Prevent OOB access.
        for (size_t i = 0; i < step_list.size(); i++)
        {
            payload_counters_[i].step = step_list[i];
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
    size_t current_counter_idx = 0;

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
                auto & payload_counter = counters_[index];
                auto & curr_counter = payload_counter[current_counter_idx];
                current_counter_idx += 1;

                uint64_t val = curr_counter.counter.fetch_add(
                                    curr_counter.step, std::memory_order_relaxed);

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
