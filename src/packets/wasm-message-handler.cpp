#include "wasm-message-handler.h"


void WASMMessageHandler::parse_body_async(std::span<const uint8_t> buffer,
                                          std::function<void()> callback) const
{

}

HeaderResult WASMMessageHandler::parse_header(std::span<const uint8_t> buffer) const
{
    if (parse_header_func)
    {
        // Use C++ lambda if available.
        return parse_header_func(buffer);
    }
    else
    {
        // Call WASM otherwise (obscure protocol).

        // TODO: WASM setup
        return parse_header_func(buffer);
    }
}

void WASMMessageHandler::set_header_parser(HeaderParseFunction parser)
{
    parse_header_func = std::move(parser);
}
