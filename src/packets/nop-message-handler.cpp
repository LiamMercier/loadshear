#include "nop-message-handler.h"

void NOPMessageHandler::parse_message(std::span<const uint8_t> header,
                                      std::span<const uint8_t> body,
                                      std::function<void(ResponsePacket)> callback)
                                                                              const
{
    static const std::shared_ptr<std::vector<uint8_t>> unused
        = std::make_shared<std::vector<uint8_t>>();

    callback({std::move(unused)});
}

HeaderResult NOPMessageHandler::parse_header(std::span<const uint8_t> buffer) const
{
    return {0, HeaderResult::Status::OK};
}
