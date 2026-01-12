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

#include "wasm-contract.h"

extern unsigned char __heap_base[];
static uint32_t heap_top = 0;

// TODO: improve this example to show some safer ways of writing scripts

// just move the pointer each time
uint32_t alloc(uint32_t input_size)
{
    if (heap_top == 0)
    {
        heap_top = (uint32_t)(uintptr_t)__heap_base;
    }

    uint32_t res = heap_top;
    heap_top = heap_top + input_size;

    return res;
}

uint64_t handle_body(uint32_t input_index, uint32_t input_size)
{
    uint8_t *memory = (uint8_t *)(uintptr_t)input_index;
    for (uint32_t i = 0; i < input_size; i++)
    {
        memory[i] = 0x55;
    }

    uint32_t out_ptr = input_index;
    uint32_t out_length = input_size;

    // Combine and send (order is based on the wasm contract).
    uint64_t packed = ((uint64_t)out_length << 32) | (uint64_t)out_ptr;

    return packed;
}

// Move the pointer back to the start, we are free to write over memory.
void dealloc(uint32_t input_index, uint32_t input_size)
{
    heap_top = (uint32_t)(uintptr_t)__heap_base;
}

// No-op, override by basic header parse settings.
uint32_t handle_header(uint32_t input_index, uint32_t input_size)
{
    (void)input_index;
    (void)input_size;
    return 0;
}
