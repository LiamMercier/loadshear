#pragma once

#include <memory>
#include <vector>

// TODO <optimization>: We would prefer to use a memory pool instead of passing
//                      around memory that was freshly allocated.
struct ResponsePacket {
    std::shared_ptr<std::vector<uint8_t>> packet;

    inline const uint8_t * data()
    {
        return packet->data();
    }

    inline size_t size()
    {
        return packet->size();
    }
};
