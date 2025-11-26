#pragma once

#include "payload-structs.h"

class PayloadManager
{
public:
    // TODO: decide on payload return for the session classes.

    // Compute any runtime changes to packets and return the data to caller.
    void fill_payload(size_t index, PreparedPayload & payload);

private:
    std::vector<PayloadDescriptor> payloads_;
    std::vector<PayloadCounter> counters_;
};
