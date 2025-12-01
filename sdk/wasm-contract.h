#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    // alloc must return a pointer to contiguous memory of requested size.
    uint32_t alloc(uint32_t input_size);

    // handle_body is given a pointer to the start of the payload and the payload size.
    //
    // the return must be a uint32_t pointer and size packed into a uint64_t
    uint64_t handle_body(uint32_t input_ptr, uint32_t input_size);

    // dealloc can do whatever you wish, if you do dynamic allocation you should
    // probably release memory or reuse it for the next alloc write.
    //
    // Most protocols will just have this reset a pointer to where it was before alloc was called.
    void dealloc(uint32_t input_ptr, uint32_t input_size);

    // handle_header follows the same logic as handle_body except it returns the size
    // of the packet body.
    //
    // If you decide to use a default or non-WASM defined header function, this is optional,
    // you can make this a no-op and simply not export it.
    uint32_t handle_header(uint32_t input_ptr, uint32_t input_size);

#ifdef __cplusplus
}
#endif
