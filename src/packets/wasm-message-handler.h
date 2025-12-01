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
    WASMMessageHandler(std::shared_ptr<wasmtime::Engine> engine,
                       std::shared_ptr<wasmtime::Module> module);

    // Input buffer + callback.
    //
    // The header and the body are given to the user's WASM script.'
    void parse_message(std::span<const uint8_t> header,
                       std::span<const uint8_t> body,
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

    // Engine and module are thread safe
    std::shared_ptr<wasmtime::Engine> engine_;
    std::shared_ptr<wasmtime::Module> module_;

    std::optional<wasmtime::Memory> memory_;

    // Functions we expect to exist
    std::optional<wasmtime::Func> alloc_;
    std::optional<wasmtime::Func> dealloc_;
    std::optional<wasmtime::Func> handle_body_;
    std::optional<wasmtime::Func> handle_header_;

    // These cannot be shared across threads, so we must have a MessageHandler per thread.
    mutable wasmtime::Store store_;
    std::optional<wasmtime::Instance> instance_;
};
