// Copyright (c) 2026 Liam Mercier
//
// This file is part of Loadshear.
//
// Loadshear is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License Version 3.0
// as published by the Free Software Foundation.
//
// Loadshear is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License v3.0
// for more details.
//
// You should have received a copy of the GNU General Public License v3.0
// along with Loadshear. If not, see <https://www.gnu.org/licenses/gpl-3.0.txt>

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
    // TODO <feature>: allow the user to set index values instead of forcing WASM reads.
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
