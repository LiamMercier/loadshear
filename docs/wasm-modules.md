# Host-Guest WASM Contract

The script you provide MUST follow the Host-Guest API contract for proper runtime behavior. Implementation details can be found at [WASMMessageHandler](../src/packets/wasm-message-handler.cpp). If your custom WASM module does not follow this contract, the program may have errors.

## Definitions

- The Host is the C++ code responsible for the application logic
- The Guest is the WASM module you provide and the current WASM instance it is running on

## Assumptions

- The (alloc -> handle_body -> dealloc) loop is synchronous. This loop structure is also used for reading headers, if defined by the module.
- Each WASM instance is local to the thread it is running on
- Parsing calls for two packets owned by a session will never interleave
- If the Guest has an error (Traps, exceptions) the Host will try (but may fail) to catch the error and immediately callback with an empty response. Guests should try to ensure the contract is followed for correctness regardless of Host safeguards.

## Contract

- (1): The Guest module MUST export WASM memory, a function `alloc(i32 size)->(i32 index)`, a function `dealloc(i32 index, i32 size)->()`, and a function `handle_body(i32 index, i32 size)->(i64 packed)`
    - (1.1): The Guest module MUST NOT assume that memory from the last packet parsing loop will still exist for the next packet, but it may assume memory from the previous function in the loop is stable
    - (1.2): The alloc function MUST provide a valid 32-bit offset to contiguous memory with enough room for the payload to be written. 
    - (1.3): The Guest MAY decide to export handle_header(i32 index, i32 size)->(i32 size) for reading the header
- (2): The Host MUST start parsing the packet by calling alloc(input_length)
    - (2.1): The Host MUST treat index 0 as an allocation failure and callback with an empty response.
- (3): The Host MUST then copy the packet header and body into the provided index into the Guest's linear memory
- (4): The Host MUST then call handle_body with the provided index and original input size
    - (4.1): The Guest MUST return a 64-bit integer with the lowest 32-bits as an index to the output (response) buffer and the highest 32-bits as a size
    - (4.2): The Guest MUST return size as 0 if handle_body failed
- (5): The Host MUST unpack a 64-bit integer into a 32-bit index and 32-bit size using the scheme from (4.1)
- (6): The Host MUST copy the memory for the response using the provided index and size
- (7): The Host MUST call dealloc on (output_index, output_size) if output_size is positive, and then the Host MUST call dealloc on (input_index, input_size)

# Script Examples

You can find examples in the sdk directory. A copy of the contract functions in C is provided by [wasm-contract.h](../sdk/wasm/wasm-contract.h) for convenience.

## C and C++

There are some examples in the sdk directory for compiling C and C++ functions using clang. You can compile your module using this command, possibly with -O3 instead of -O2 depending on your needs.

```
clang --target=wasm32 -O2 -nostdlib -fno-builtin-memset -Wl,--no-entry -Wl,--export=alloc -Wl,--export=dealloc -Wl,--export=handle_body -Wl,--initial-memory=131072 -o packet-parser.wasm <YOUR_CODE>.c
```

If you define handle_header for your module, export this as well.

```
clang --target=wasm32 -O2 -nostdlib -fno-builtin-memset -Wl,--no-entry -Wl,--export=alloc -Wl,--export=dealloc -Wl,--export=handle_body -Wl,--export=handle_header -Wl,--initial-memory=131072 -o packet-parser.wasm <YOUR_CODE>.c
```

Optionally, strip debug information with the WebAssembly Binary Toolkit (WABT).

```
wasm-strip <YOUR_CODE>.wasm
```

TODO: show a respond heartbeat example
