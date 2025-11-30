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

uint64_t handle_body(uint32_t input_ptr, uint32_t input_size)
{
    uint8_t *memory = (uint8_t *)(uintptr_t)input_ptr;
    for (uint32_t i = 0; i < input_size; i++)
    {
        memory[i] = 0x55;
    }

    uint32_t out_ptr = input_ptr;
    uint32_t out_length = input_size;

    // Combine and send (order is based on the wasm contract).
    uint64_t packed = ((uint64_t)out_length << 32) | (uint64_t)out_ptr;

    return packed;
}

// Move the pointer back to the start, we are free to write over memory.
void dealloc(uint32_t input_ptr, uint32_t input_size)
{
    heap_top = (uint32_t)(uintptr_t)__heap_base;
}
