#pragma once

#include <functional>

#include <wasmtime.hh>

#include "message-handler-interface.h"

class WASMMessageHandler : public MessageHandler
{
public:
    using HeaderParseFunction = std::function<HeaderResult(std::span<const uint8_t>)>;

// Overrides
public:
    void parse_body_async(std::span<const uint8_t> buffer,
                          std::function<void()> callback) const override;

    HeaderResult parse_header(std::span<const uint8_t> buffer) const override;

    ~WASMMessageHandler() = default;

public:
    void set_header_parser(HeaderParseFunction parser);

private:
    HeaderParseFunction parse_header_func;
};
