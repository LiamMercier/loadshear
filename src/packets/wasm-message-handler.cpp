#include "wasm-message-handler.h"

// TODO: Call WASM, and copy the entire result out so the call is stateless.
void WASMMessageHandler::parse_body_async(std::span<const uint8_t> buffer,
                                          std::function<void(ResponsePacket)> callback) const
{
    // TODO: call wasmtime

    // TODO: get base pointer and length

    // uint8_t *base = ;
    // size_t base_len = ;

    // Copy the data from wasmtime.
    // auto vec = std::make_shared<std::vector<uint8_t>>(base, base + base_len);

    // TEMPORARY: remove this when we implement wasmtime properly.
    static const uint8_t temp_data[] = { 0x01, 0x02, 0x03, 0x04 };
    auto vec = std::make_shared<std::vector<uint8_t>>(temp_data,
                                                      temp_data + sizeof(temp_data));

    // Callback into our Session.
    callback({std::move(vec)});
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
