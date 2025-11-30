#pragma once

#include "header-result.h"
#include "response-packet.h"

struct MessageHandler
{
public:
    virtual void parse_message(std::span<const uint8_t> header,
                               std::span<const uint8_t> body,
                               std::function<void(ResponsePacket)> callback) const = 0;

    virtual HeaderResult parse_header(std::span<const uint8_t> buffer) const = 0;

    virtual ~MessageHandler() = default;

private:

};
