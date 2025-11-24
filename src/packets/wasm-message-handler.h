#pragma once

#include <functional>

#include <wasmtime.hh>

#include "message-handler-interface.h"

class WASMMessageHandler : MessageHandler
{
public:
    void parse_body_async(std::span<const uint8_t> buffer,
                          std::function<void()> callback) override;

    void parse_header(std::span<const uint8_t> buffer) override;

    ~WASMMessageHandler() = default;
private:

};
