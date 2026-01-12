// SPDX-License-Identifier: BSL-1.0
//
// Copyright (c) 2026 Liam Mercier
//
// This file is released under the Boost Software License - Version 1.0

#pragma once
#include <stdint.h>

#ifdef __cplusplus
#define SET_CPP_NOEXCEPT noexcept
extern "C" {
#else
#define SET_CPP_NOEXCEPT
#endif

    // alloc must return an index to contiguous memory of requested size.
    uint32_t alloc(uint32_t input_size) SET_CPP_NOEXCEPT;

    // handle_body is given an index to the start of the payload and the payload size.
    //
    // the return must be a uint32_t index and uint32_t size packed into a uint64_t
    uint64_t handle_body(uint32_t input_index, uint32_t input_size) SET_CPP_NOEXCEPT;

    // dealloc can do whatever you wish, if you do dynamic allocation you should
    // probably release memory or reuse it for the next alloc write.
    //
    // Most protocols will just have this reset a pointer to where it was before alloc was called.
    void dealloc(uint32_t input_index, uint32_t input_size) SET_CPP_NOEXCEPT;

    // handle_header follows the same logic as handle_body except it returns the size
    // of the packet body.
    //
    // If you decide to use a default or non-WASM defined header function, this is optional,
    // you can make this a no-op and simply not export it.
    uint32_t handle_header(uint32_t input_index, uint32_t input_size) SET_CPP_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#undef SET_CPP_NOEXCEPT
