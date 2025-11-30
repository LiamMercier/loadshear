#include "wasm-message-handler.h"

#include <utility>
#include <iostream>

// for memcpy
#include <cstring>

WASMMessageHandler::WASMMessageHandler(std::shared_ptr<wasmtime::Engine> engine,
                                       std::shared_ptr<wasmtime::Module> module)
:engine_(std::move(engine)),
module_(std::move(module)),
store_(*engine_)
{
    auto tmp_instance = wasmtime::Instance::create(store_, *module_, {});

    if (!tmp_instance)
    {
        // We cannot proceed with execution, the program must end and caller must deal with the error.
        throw std::runtime_error("WASM instance could not be created! Aborting!");
    }

    instance_ = std::move(tmp_instance.unwrap());

    auto maybe_memory = instance_->get(store_, "memory");

    // Memory didn't exist, exit.
    if (!maybe_memory)
    {
        throw std::runtime_error("WASM memory could not be created! Aborting!");
    }

    // We should basically always get Memory, but this is a good defensive that we only pay for
    // during the construction of a new handler (very rare).
    auto mem_ptr = std::get_if<wasmtime::Memory>(&*maybe_memory);

    if (!mem_ptr)
    {
        throw std::runtime_error("WASM memory could not be created! Aborting!");
    }

    memory_ = std::move(*mem_ptr);

    //
    // Grab functions from instance.
    //

    // Try to get alloc, fail if we can't.
    auto maybe_alloc = instance_->get(store_, "alloc");

    // If we don't have alloc, stop now.
    if (!maybe_alloc)
    {
        throw std::runtime_error("WASM module does not export working alloc! Aborting!");
    }

    auto alloc_ptr = std::get_if<decltype(alloc_)::value_type>(&*maybe_alloc);

    if (!alloc_ptr)
    {
        throw std::runtime_error("WASM module does not export working alloc! Aborting!");
    }

    alloc_ = std::move(*alloc_ptr);

    // Try to get dealloc, fail if we can't.
    auto maybe_dealloc = instance_->get(store_, "dealloc");

    // If we can't find this, stop.
    if (!maybe_dealloc)
    {
        throw std::runtime_error("WASM module does not export working dealloc! Aborting!");
    }

    auto dealloc_ptr = std::get_if<decltype(dealloc_)::value_type>(&*maybe_dealloc);

    if (!dealloc_ptr)
    {
        throw std::runtime_error("WASM module does not export working dealloc! Aborting!");
    }

    dealloc_ = std::move(*dealloc_ptr);

    // Try to get handle_body, fail if we can't.
    auto maybe_body = instance_->get(store_, "handle_body");

    if (!maybe_body)
    {
        throw std::runtime_error("WASM module does not export working handle_body! Aborting!");
    }

    auto body_ptr = std::get_if<decltype(handle_body_)::value_type>(&*maybe_body);

    if (!body_ptr)
    {
        throw std::runtime_error("WASM module does not export working handle_body! Aborting!");
    }

    handle_body_ = std::move(*body_ptr);
}

// TODO: consider safety improvements, modify contract a bit if needed.

// We synchronously call the WASM code (runtime is embedded, low overhead). We assume that
// the user functions will not be computationally prohibitive.
void WASMMessageHandler::parse_body_async(std::span<const uint8_t> buffer,
                                          std::function<void(ResponsePacket)> callback) const
{
    // Annotated with the required Host <-> Guest API contract.
    //
    // (CONTRACT 1) was already resolved, we found the required exports in the module.
    try
    {
        // (CONTRACT 2): Allocate in the user's module.
        int32_t input_length = static_cast<int32_t>(buffer.size());

        auto alloc_res = alloc_->call(store_, {input_length}).unwrap();

        int32_t input_ptr = alloc_res[0].i32();

        // Bad allocation.
        //
        // TODO: document this in contract explanation/file. (bad alloc -> ptr is zero)
        if (input_ptr == 0 && input_size != 0)
        {
            std::cout << "Bad allocation\n";
            callback({ std::make_shared<std::vector<uint8_t>>() });
            return;
        }

        // (CONTRACT 3): Copy data from Host to Guest
        uint8_t *guest_memory = memory_->data(store_).data();

        std::memcpy(guest_memory + input_ptr, buffer.data(), buffer.size());

        // (CONTRACT 4): Host calls the required handler from Guest.
        auto body_res = handle_body_->call(store_, {input_ptr, input_length}).unwrap();

        int64_t packed = body_res[0].i64();

        // (CONTRACT 5): Host interprets lower 32 bits as the pointer, upper 32 as size.
        int32_t out_ptr = static_cast<uint32_t>(packed & 0xffffffu);
        int32_t out_length = static_cast<uint32_t>((packed >> 32) & 0xffffffffu);

        // (CONTRACT 6): Copy data from Guest to Host.
        auto vec = std::make_shared<std::vector<uint8_t>>();

        if (out_length > 0)
        {
            // Handle guests changing their memory layout.
            guest_memory = memory_->data(store_).data();
            out_ptr = guest_memory + out_ptr;

            // Copy data out of guest.
            vec->resize(out_length);
            std::memcpy(vec->data(), guest_memory + out_ptr, out_length);

            // Call dealloc
            dealloc_->call(store_, {out_ptr, out_length});
        }

        // (CONTRACT 7): Host calls deallocate for Guest.

        // Grab memory pointer if anything changed.
        guest_memory = memory_->data(store_).data();

        // Deallocate.
        dealloc_->call(store_, {input_ptr, input_length}).unwrap();

        // We assume that the Guest will deal with its own allocated buffer.
        callback({std::move(vec)});
        return;
    }
    catch (const wasmtime::Trap & error)
    {
        std::cerr << "WASM Trap: " << error.message() << "\n";
        auto vec = std::make_shared<std::vector<uint8_t>>();
        callback({std::move(vec)});
        return;
    }
    catch (const std::exception & error)
    {
        std::cerr << "WASM exception: " << error.what() << "\n";
        auto vec = std::make_shared<std::vector<uint8_t>>();
        callback({std::move(vec)});
        return;
    }
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
