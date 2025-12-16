#pragma once

#include <functional>
#include <span>

#include "message-handler-interface.h"

class NOPMessageHandler : public MessageHandler
{
public:
    using HeaderParseFunction = std::function<HeaderResult(
                                        std::span<const uint8_t>)>;

    // Do nothing.
    void parse_message(std::span<const uint8_t> header,
                       std::span<const uint8_t> body,
                       std::function<void(ResponsePacket)> callback) const override;

    HeaderResult parse_header(std::span<const uint8_t> buffer) const override;

public:

};
