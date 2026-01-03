#pragma once

#include "payload-structs.h"

class PayloadManager
{
public:
    // We expect a list of payload descriptors, and for each payload descriptor
    // we expect a index matched list of counter step values.
    //
    // So, for payload descriptor 1, we expect a vector of uint16_t with one value
    // per COUNTER declared in the underlying SEND operation.
    PayloadManager(std::vector<PayloadDescriptor> payloads,
                   std::vector<std::vector<uint16_t>> steps);

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
    mutable std::vector<std::vector<PayloadCounter>> counters_;
};
