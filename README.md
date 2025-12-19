# Loadshear

## about

TODO: about

## table of contents

TODO: table of contents

TODO: split this readme into Users and Developers for ease of navigation

## Compiling

Download build tools and libraries

```
sudo apt install build-essential cmake libboost-all-dev
```

Download Wasmtime

```
https://github.com/bytecodealliance/wasmtime
```

Download a compatible rust version for compiling wasmtime.

```
sudo apt install rustup
```

```
rustup default stable
```

Compile the binary

```
cmake --preset release && cmake --build --preset release-multi
```

For debugging, use

```
cmake --preset debug && cmake --build --preset debug-multi
```

To run all tests with ctest, set ulimit to a high enough value (~12000) and run

```
ctest --preset debug-heavy
```

## Defining the packet response protocol

The application assumes that packets are started with a fixed size header that contains the body size of the packet. For example, a packet might have the first 8 bytes as HeaderType:BodySize and then the rest as the body.

If your protocol uses variable sized headers, you must declare the header size to be the first N bytes and make sure that this will always contain the size field.

Each Session instance (with reading enabled) will repeatedly do:

- 1. Try to read your predefined HEADER_SIZE bytes from the socket
- 2. Try to get the body size by calling the user defined handle_header function on the bytes read
- 3. Try to read body_size bytes to get the body
- 4. Try to generate a response by giving the header and body as contiguous memory to the defined MessageHandler instance for the current test
- 5. Restart the loop

### Message Handlers

Each Session that is set to respond to messages must have a MessageHandler instance it uses.

Currently, there is only one method of handling messages, which is the WASMMessageHandler instance t hat gets created at the start of each test you define. To use this, you must provide:

- A WASM module that defines how to read the body
- A handle_header function in the module to read the body size from headers, or a list of index values (defined in the config) to read to construct body size.

### Scripting and message handling

Users can supply custom functions which generate a response to server packets. To do this, you need to create a WebAssembly (WASM) module which fulfills the Host-Guest API contract for this program. If your custom WASM module does not follow this contract, the program may have errors.

If your protocol has simple header parsing semantics, you can avoid creating a handle_body function in your WASM module and simply provide a set of index values to read the size from the header. The program will use the index values first for header parsing if they were provided.

### Host-Guest WASM contract

The script you provide must follow this contract for proper runtime behavior. For implementation specific details, please see [WASMMessageHandler](src/packets/wasm-message-handler.cpp) for details.

#### Definitions

- The Host is the C++ code responsible for the application logic
- The Guest is the WASM module you provide and the current WASM instance it is running on

#### Assumptions

You can create your script under the assumption that the (alloc -> handle_body -> dealloc) loop is synchronous. This loop structure is also used for reading headers, if defined by the module.

- Each WASM instance is local to the thread it is running on
- Parsing calls for two packets will never interleave
- If the Guest has an error (Traps, exceptions) the Host will try (but may fail) to catch the error and immediately callback with an empty response. Guests should try to ensure the contract is followed for correctness regardless of Host safeguards.

#### Contract

- (1): The Guest module MUST export WASM memory, a function `alloc(i32 size)->(i32 index)`, a function `dealloc(i32 index, i32 size)->()`, and a function `handle_body(i32 index, i32 size)->(i64 packed)`
    - (1.1): The Guest module MUST NOT assume that memory from the last packet parsing loop will still exist for the next packet, but it may assume memory from the previous function in the loop is stable
    - (1.2): The alloc function MUST provide a valid 32-bit offset to contiguous memory with enough room for the necessary payload to be written. 
    - (1.3): The Host MUST treat 0 as an allocation failure and callback with an empty response if the input size is positive.
    - (1.4): The Guest MAY decide to implement dealloc to do any cleanup logic needed, or as a no-op
    - (1.5): The Guest MAY decide to export handle_header(i32 index, i32 size)->(i32 size) for reading the header, which will be used like handle_body to get the packet's body size
- (2): The Host MUST start parsing the packet by calling alloc(input_length)
- (3): The Host MUST then copy the packet header and body into the provided address
- (4): The Host MUST then call handle_body with the provided address and original input size
    - (4.1): The Guest MUST return a 64-bit integer with the lowest 32-bits as an offset to the output (response) buffer and the highest 32-bits as a size
    - (4.2): The Guest MUST return size as 0 if handle_body failed
- (5): The Host MUST unpack a 64-bit integer into a 32-bit index and 32-bit size using the scheme from (4.1)
- (6): The Host MUST copy the memory for the response using the provided index and size
- (7): The Host MUST call dealloc on (output_index, output_size) if output_size is positive, and then the Host MUST call dealloc on (input_index, input_size)

### Script examples

You can find examples in the sdk directory. A copy of the contract functions in C is provided by [wasm-contract.h](sdk/wasm-contract.h) for convenience.

#### C and C++

There are examples for compiling C and C++ functions using clang. To export without a header read function we simply do not export handle_header

```
clang --target=wasm32 -O2 -nostdlib -fno-builtin-memset -Wl,--no-entry -Wl,--export=alloc -Wl,--export=dealloc -Wl,--export=handle_body -Wl,--initial-memory=131072 -o packet-parser.wasm <YOUR_CODE>.c
```

If you define in your WASM module how to read the header for the packet's body size, you must export handle_header as well

```
clang --target=wasm32 -O2 -nostdlib -fno-builtin-memset -Wl,--no-entry -Wl,--export=alloc -Wl,--export=dealloc -Wl,--export=handle_body -Wl,--export=handle_header -Wl,--initial-memory=131072 -o packet-parser.wasm <YOUR_CODE>.c
```

Optionally, strip debug information with the WebAssembly Binary Toolkit (WABT).

```
wasm-strip <YOUR_CODE>.wasm
```

TODO: show a respond heartbeat example

## Running tests

The tests rely on .wasm files to run. If you do not wish to use the provided binary files, feel free to compile them yourself using the examples in the sdk directory.

TODO: make a script to compile each file

Some tests are hidden behind an environment variable. You can run these with `RUN_HEAVY_GTEST=1` if you wish. Some tests may require more file descriptors than allowed by default, you may need to use `ulimit -n 8192` or something similar.
