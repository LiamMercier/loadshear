# stressor

## about

TODO: about

## table of contents

TODO: table of contents

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

Compile the binary, running test cases after compilation

```
cmake -DCMAKE_BUILD_TYPE=Debug -B builds -S . && cmake --build builds && ctest --test-dir ./builds/
```

## Defining the packet parsing protocol

Users can supply custom functions which generate a response to server packets. To do this, you need to create a WebAssembly (WASM) module which fulfills the Host-Guest API contract for this program. If your custom WASM module does not follow this contract, the program may have errors.

### Host-Guest contract.

The script you provide must follow this contract for proper runtime behavior. To see implementation specific details, please see [WASMMessageHandler](src/packets/wasm-message-handler.cpp) for details.

#### Definitions

- The Host is the C++ code responsible for the application logic
- The Guest is the WASM module you provide and the current WASM instance it is running on

#### Assumptions

You can create your script under the assumption that the (alloc -> handle_body -> dealloc) loop is synchronous.

- Each WASM instance is local to the thread it is running on
- Parsing calls for two packets will never interleave
- If the Guest has an error (Traps, exceptions) the Host will try to catch the error and immediately callback with an empty response

#### Contract

- (1): The Guest module MUST export WASM memory, a function `alloc(i32 size)->(i32 ptr)`, a function `dealloc(i32 ptr, i32 size)->()`, and a function `handle_body(i32 ptr, i32 size)->(i64 packed)`
    - (1.1): The Guest module MUST NOT assume that memory from the last packet parsing loop will still exist for the next packet, but it may assume memory from the last function in the loop is stable
    - (1.2): The alloc function MUST provide a valid 32-bit index of contiguous memory with enough room for the necessary payload to be written. 
    - (1.3): The Host MUST treat 0 as an allocation failure and callback with an empty response if the input size is positive.
    - (1.4): The Guest MAY decide to implement dealloc to do any cleanup logic needed, or as a no-op
- (2): The Host MUST start parsing the packet by calling alloc(input_length)
- (3): The Host MUST then copy the packet header and body into the provided address
- (4): The Host MUST then call handle_body with the provided address and original input size
    - (4.1): The Guest MUST return a 64-bit integer with the lowest 32-bits as a pointer to the output (response) buffer and the highest 32-bits as a size
    - (4.2): The Guest MUST return size as being 0 if handle_body failed to prevent memory from being read by the Host
- (5): The Host MUST unpack a 64-bit integer into a 32-bit pointer and 32-bit size using the scheme from (4.1)
- (6): The Host MUST copy the memory for the response using the provided pointer and size
- (7): The Host MUST call dealloc on (output_ptr, output_size) if output_size is positive, and then the Host MUST call dealloc on (input_ptr, input_size)

### Examples and tutorial

TODO: explain how to create modules for different developers

### C and C++

There is an example for compiling C and C++ using clang.

```
clang --target=wasm32 -O2 -nostdlib -fno-builtin-memset  -Wl,--no-entry -Wl,--no-producers-section -Wl,--export=alloc -Wl,--export=dealloc -Wl,--export=handle_body -Wl,--initial-memory=131072 -o packet-parser.wasm <YOUR_CODE>.c
```

Optionally, strip debug information with the WebAssembly Binary Toolkit (WABT).

```
wasm-strip <YOUR_CODE>.wasm
```

TODO: show a respond heartbeat example

## Running tests

The tests rely on .wasm files to run. If you do not wish to use the provided binary files, feel free to compile them yourself using the examples in the sdk directory.

TODO: make a script to compile each file

## Other info (temporary)

WASM modules you provide MUST be stateless (i.e functions). This is likely already the case.
