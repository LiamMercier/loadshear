#include "wasm-contract.h"

// Ensure wasm32
static_assert(sizeof(void*) == 4);

enum class HeaderType : uint8_t
{
    Login,
    Register,
    Ping,
    PingResponse,
    SendDM
};

struct Header
{
    HeaderType type_;
    uint32_t payload_len;
};

extern "C" {
    extern unsigned char __heap_base[];
}

static uint32_t heap_top = 0;

uint32_t alloc(uint32_t input_size) noexcept
{
    if (heap_top == 0)
    {
        heap_top = (uint32_t)(uintptr_t)__heap_base;
    }

    uint32_t res = heap_top;
    heap_top = heap_top + input_size;

    return res;
}

constexpr uint32_t expected_header_size = 5;

// Read the header and decide on what type it is
uint64_t handle_body(uint32_t input_index, uint32_t input_size) noexcept
{
    // If the index is 0 or the size is 0, something went wrong. We should always
    // have a valid index and the size should always at least be the header size.
    //
    // Just do nothing here.
    if (input_index == 0 || input_size < expected_header_size)
    {
        return 0;
    }

    Header body_header;

    const uint8_t *packet = reinterpret_cast<const uint8_t *>(
                                static_cast<uintptr_t>(input_index));

    // Grab type.
    body_header.type_ = static_cast<HeaderType>(packet[0]);

    // We are returning a header.
    uint32_t out_ptr = input_index;
    uint32_t out_length = expected_header_size;

    // Overwrite the input, we don't need it and our dealloc doesn't care about
    // what memory is where.
    if (body_header.type_ == HeaderType::Ping)
    {
        uint8_t *out_packet = reinterpret_cast<uint8_t *>(
                                static_cast<uintptr_t>(input_index));

        // Send ping response if we got pinged.
        out_packet[0] = static_cast<uint8_t>(HeaderType::PingResponse);

        // Empty payload.
        out_packet[1] = 0;
        out_packet[2] = 0;
        out_packet[3] = 0;
        out_packet[4] = 0;

        uint64_t packed = ((uint64_t)out_length << 32) | (uint64_t)out_ptr;

        return packed;
    }
    else
    {
        // Return nothing.
        return 0;
    }
}

// Move the pointer back to the top, we can just overwrite next time anyways.
void dealloc(uint32_t input_index, uint32_t input_size) noexcept
{
    heap_top = (uint32_t)(uintptr_t)__heap_base;
}

// We defined the header size as 5 in the calling gtest function.
uint32_t handle_header(uint32_t input_index, uint32_t input_size) noexcept
{
    if (input_size != expected_header_size || input_index == 0)
    {
        return 0;
    }

    const uint8_t *memory = reinterpret_cast<const uint8_t *>(
                                static_cast<uintptr_t>(input_index));

    // Grab payload length
    uint32_t payload_len = static_cast<uint32_t>(memory[1])
                                | (static_cast<uint32_t>(memory[2]) << 8)
                                | (static_cast<uint32_t>(memory[3]) << 16)
                                | (static_cast<uint32_t>(memory[4]) << 24);

    return payload_len;
}
