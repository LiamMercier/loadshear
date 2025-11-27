#pragma once

#include <functional>
#include <optional>

#include <wasmtime.hh>

#include "message-handler-interface.h"

class WASMMessageHandler : public MessageHandler
{
public:
    using HeaderParseFunction = std::function<HeaderResult(std::span<const uint8_t>)>;

// Overrides
public:
    // Input buffer + callback.
    void parse_body_async(std::span<const uint8_t> buffer,
                          std::function<void(ResponsePacket)> callback) const override;

    HeaderResult parse_header(std::span<const uint8_t> buffer) const override;

    ~WASMMessageHandler() = default;

public:
    void set_header_parser(HeaderParseFunction parser);

private:
    HeaderParseFunction parse_header_func;

    //
    // WASM related members.
    //

    // Members that cannot be shared across threads.
    // We want multiple to increase throughput.
    static thread_local wasmtime::Store store_;
    static thread_local wasmtime::Instance instance_;

    // Engine and module are thread safe
    std::shared_ptr<wasmtime::Engine> engine_;
    std::shared_ptr<wasmtime::Module> module_;

    std::optional<wasmtime::Memory> memory_;
    std::optional<wasmtime::Func> wasm_handle_request_;
};
