#pragma once

#include "payload-structs.h"

class PayloadManager
{
public:
    PayloadManager(std::vector<PayloadDescriptor> payloads,
                   std::vector<uint16_t> steps);

    // Compute any runtime changes to packets and return the data to caller.
    //
    // Returns false if no payload exists.
    bool fill_payload(size_t index, PreparedPayload & payload) const;

private:
    void write_numeric(uint8_t *start,
                       uint64_t raw_numeric,
                       uint32_t length,
                       bool little_endian) const;

    std::vector<PayloadDescriptor> payloads_;
    mutable std::vector<PayloadCounter> counters_;
};
