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

    // Try to get handle_header, otherwise we will use the header function provided.
    auto maybe_header = instance_->get(store_, "handle_header");

    if (!maybe_header)
    {
        return;
    }

    auto header_ptr = std::get_if<decltype(handle_header_)::value_type>(&*maybe_body);

    if (!header_ptr)
    {
        return;
    }

    handle_header_ = std::move(*header_ptr);

    // Set the header parsing function to use the WASM header.

    set_header_parser([this](std::span<const uint8_t> buffer) -> HeaderResult {
        try
        {
            // Call WASM otherwise (obscure protocol).
            uint32_t input_length = static_cast<uint32_t>(buffer.size());

            auto alloc_res = alloc_->call(store_, {static_cast<int32_t>(buffer.size())}).unwrap();

            uint32_t input_ptr = alloc_res[0].i32();

            if (input_ptr == 0)
            {
                std::cout << "Bad allocation detected for header\n";

                dealloc_->call(store_,
                               {static_cast<int32_t>(input_ptr),
                                static_cast<int32_t>(input_length)}).unwrap();

                return {0, HeaderResult::Status::ERROR};
            }

            auto mem_view = memory_->data(store_);
            uint8_t *guest_memory = mem_view.data();

            if (static_cast<uint64_t>(input_ptr) + static_cast<uint64_t>(input_length)
            > mem_view.size())
            {
                std::cout << "OOB behavior detected during header input buffer write. "
                          << "Your WASM script violates the contract.\n";

                dealloc_->call(store_,
                               {static_cast<int32_t>(input_ptr),
                                static_cast<int32_t>(input_length)}).unwrap();

                return {0, HeaderResult::Status::ERROR};
            }

            std::memcpy(guest_memory + input_ptr,
                        buffer.data(),
                        buffer.size());

            auto header_res = handle_header_->call(store_,
                                                   {static_cast<int32_t>(input_ptr),
                                                    static_cast<int32_t>(input_length)}).unwrap();

            uint32_t size = 0;
            int32_t signed_size = header_res[0].i32();

            if (signed_size < 0)
            {
                // Handle bad sizes
                std::cout << "handle_header returned a negative size. "
                          << "Your WASM script violates the contract.\n";

                dealloc_->call(store_,
                               {static_cast<int32_t>(input_ptr),
                                static_cast<int32_t>(input_length)}).unwrap();

                return {size, HeaderResult::Status::ERROR};
            }

            size = static_cast<uint32_t>(signed_size);

            // Call dealloc
            dealloc_->call(store_,
                           {static_cast<int32_t>(input_ptr),
                            static_cast<int32_t>(input_length)}).unwrap();

            return {size, HeaderResult::Status::OK};
        }
        catch (...)
        {
            std::cout << "Exception during header input buffer write. "
                      << "Your WASM script violates the contract.\n";
            return {0, HeaderResult::Status::ERROR};
        }
    });
}

// We synchronously call the WASM code (runtime is embedded, low overhead). We assume that
// the user functions will not be computationally prohibitive.
void WASMMessageHandler::parse_message(std::span<const uint8_t> header,
                                       std::span<const uint8_t> body,
                                       std::function<void(ResponsePacket)> callback) const
{
    // Annotated with the required Host <-> Guest API contract.
    //
    // (CONTRACT 1) was already resolved, we found the required exports in the module.
    try
    {
        // (CONTRACT 2): Allocate in the user's module.

        // Compute length. This is assumed to not overflow, any packet this large is unreasonable.
        uint32_t input_length = static_cast<uint32_t>(header.size() + body.size());

        auto alloc_res = alloc_->call(store_, {static_cast<int32_t>(input_length)}).unwrap();

        uint32_t input_ptr = alloc_res[0].i32();

        // Bad allocation if pointer is zero.
        if (input_ptr == 0 && input_length != 0)
        {
            // TODO: log with a log level? Also, remove \n if we do.
            std::cout << "Bad allocation detected for body\n";

            dealloc_->call(store_,
                           {static_cast<int32_t>(input_ptr),
                            static_cast<int32_t>(input_length)}).unwrap();

            callback({ std::make_shared<std::vector<uint8_t>>() });
            return;
        }

        // (CONTRACT 3): Copy data from Host to Guest
        auto mem_view = memory_->data(store_);
        uint8_t *guest_memory = mem_view.data();

        // Try to prevent OOB memory access.
        if (static_cast<uint64_t>(input_ptr) + static_cast<uint64_t>(input_length)
            > mem_view.size())
        {
            // TODO: log?
            std::cout << "OOB behavior detected during input buffer write. "
                      << "Your WASM script violates the contract.\n";

            dealloc_->call(store_,
                           {static_cast<int32_t>(input_ptr),
                            static_cast<int32_t>(input_length)}).unwrap();

            callback({ std::make_shared<std::vector<uint8_t>>() });
            return;
        }

        std::memcpy(guest_memory + input_ptr,
                    header.data(),
                    header.size());

        std::memcpy(guest_memory + input_ptr + header.size(),
                    body.data(),
                    body.size());

        // (CONTRACT 4): Host calls the required handler from Guest.
        auto body_res = handle_body_->call(store_,
                                           {static_cast<int32_t>(input_ptr),
                                            static_cast<int32_t>(input_length)}).unwrap();

        uint64_t packed = body_res[0].i64();

        // (CONTRACT 5): Host interprets lower 32 bits as the pointer, upper 32 as size.
        uint32_t out_ptr = static_cast<uint32_t>(packed & 0xffffffffu);
        uint32_t out_length = static_cast<uint32_t>((packed >> 32) & 0xffffffffu);

        // (CONTRACT 6): Copy data from Guest to Host.
        auto vec = std::make_shared<std::vector<uint8_t>>();

        if (out_length > 0)
        {
            // Handle guests changing their memory layout.
            mem_view = memory_->data(store_);
            guest_memory = memory_->data(store_).data();

            // Try to prevent OOB memory access.
            if (static_cast<uint64_t>(out_ptr) + static_cast<uint64_t>(out_length)
                > mem_view.size())
            {
                // TODO: log?
                std::cout << "OOB behavior detected during response buffer read. "
                          << "Your WASM script violates the contract.\n";

                dealloc_->call(store_,
                               {static_cast<int32_t>(out_ptr),
                                static_cast<int32_t>(out_length)}).unwrap();

                dealloc_->call(store_,
                               {static_cast<int32_t>(input_ptr),
                                static_cast<int32_t>(input_length)}).unwrap();

                callback({ std::make_shared<std::vector<uint8_t>>() });
                return;
            }

            // Copy data out of guest.
            vec->resize(out_length);
            std::memcpy(vec->data(), guest_memory + out_ptr, out_length);

        // (CONTRACT 7): Host calls deallocate for Guest.

            // Call dealloc on output buffer.
            dealloc_->call(store_,
                           {static_cast<int32_t>(out_ptr),
                            static_cast<int32_t>(out_length)}).unwrap();
        }

        // Call dealloc on input buffer.
        dealloc_->call(store_,
                       {static_cast<int32_t>(input_ptr),
                        static_cast<int32_t>(input_length)}).unwrap();

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
        std::cout << "No header parse function was found! Either provide a WASM "
                  << "handle_header export or provide byte fields to read in config!\n";
        return {0, HeaderResult::Status::ERROR};
    }
}

void WASMMessageHandler::set_header_parser(HeaderParseFunction parser)
{
    parse_header_func = std::move(parser);
}
