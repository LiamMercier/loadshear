# Loadshear

TODO: about

## table of contents

TODO: table of contents

## Installation

TODO: installation guide (should be simple when done).

## Quickstart

Lets quickly learn how to use `loadshear` for generating network loads.

Using `loadshear` requires a valid loadshear script. In this tutorial, we will use a simple template script to demonstrate functionality. Start by creating a new folder.

`mkdir ex-loadshear && cd ex-loadshear`

Make a placeholder packet.

`printf "hello world" > packet.bin`

Copy the example template [template.ldsh](sdk/scripts/template.ldsh) into the folder.

Now, view the execution plan using the dry run flag.

`loadshear template.ldsh --dry-run`

Or, try running the script against your computer's loopback address.

`loadshear template.ldsh`

## Loadshear Scripting Language

Loadshear uses a simple Domain Specific Language (DSL) named Loadshear Script for customizing load generation. The `loadshear` application will use any file that follows the DSL, but we suggest naming files with `.ldsh` or `.loadshear` for workspace cleanliness. Examples can be found in the [sdk](sdk) folder.

### Syntax

Valid scripts are expected to have:

- One `SETTINGS` block, defining any necessary settings or files
- One `ORCHESTRATOR` block, defining executable actions

In the event that there are multiple blocks the program may output an error or use the last block as being canonical. 

> Note: In the future, we may extend the DSL to allow for multiple block pairs.

TODO:

### Execution Model

High level blocks are parsed to create an execution plan which can be previewed with the flag `--dry-run` from the CLI. 

During plan generation, we read each packet defined in `PACKETS` into memory, and generate a small payload descriptor for each `SEND` copy that was defined.

## Defining the Packet Response Protocol

The application assumes that incoming packets are started with a fixed size header containing the body size of the packet. For example, a packet might have the first 8 bytes defining the packet type and body length, and then the body.

If your protocol uses variable sized headers, you must declare HEADERSIZE such that it will always contain the size field to extract the body.

Each session instance (with reading enabled) will:

- 1. Try to read HEADERSIZE bytes from the socket
- 2. Try to get the packet's body_size by calling the user defined handle_header function on the bytes read
- 3. Try to read body_size bytes from the socket to get the body
- 4. Try to generate a response by giving the header and body as contiguous memory to the defined MessageHandler instance
- 5. Send the response and restart the loop

### Message Handlers

Each Session that reads messages MUST be assigned a MessageHandler.

There are two options for HANDLER to use:

1) Set HANDLER = "NOP" in your script. The session will clear incoming data without invoking response logic.

2) Use a WebAssembly (WASM) module passed to HANDLER as a file path, just like any packets you have defined. To use the WASM handler, you must provide:

- A valid .wasm module that defines how to read the body
- A function in the module to read the body size from headers, or a list of index values (defined in the config) to read for constructing the body size.

### Host-Guest WASM Contract

The script you provide MUST follow the Host-Guest API contract for proper runtime behavior. Implementation details can be found at [WASMMessageHandler](src/packets/wasm-message-handler.cpp). If your custom WASM module does not follow this contract, the program may have errors.

#### Definitions

- The Host is the C++ code responsible for the application logic
- The Guest is the WASM module you provide and the current WASM instance it is running on

#### Assumptions

- The (alloc -> handle_body -> dealloc) loop is synchronous. This loop structure is also used for reading headers, if defined by the module.
- Each WASM instance is local to the thread it is running on
- Parsing calls for two packets owned by a session will never interleave
- If the Guest has an error (Traps, exceptions) the Host will try (but may fail) to catch the error and immediately callback with an empty response. Guests should try to ensure the contract is followed for correctness regardless of Host safeguards.

#### Contract

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

### Script Examples

You can find examples in the sdk directory. A copy of the contract functions in C is provided by [wasm-contract.h](sdk/wasm/wasm-contract.h) for convenience.

#### C and C++

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

## Development

The following documents useful information for developers or power users who wish to compile Loadshear.

### Compiling

Download build tools and libraries

```
sudo apt install build-essential cmake libboost-all-dev
```

Download Wasmtime, as of writing it seems they are using a shell script.

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

Compile the binary for release

```
cmake --preset release && cmake --build --preset release-multi
```

Or, to compile for debugging, use

```
cmake --preset debug && cmake --build --preset debug-multi
```

To run all tests with ctest, set ulimit to a high enough value (~12000) and run

```
ctest --preset debug-heavy
```

Or omit socket heavy tests

```
ctest --preset debug
```

### Running Tests

The tests rely on .wasm files to run. If you do not wish to use the provided binary files, feel free to compile them yourself using the examples in the sdk directory.

TODO: make a script to compile each file

Some tests are hidden behind an environment variable. You can run these with `RUN_HEAVY_GTEST=1` if you wish. Some tests may require more file descriptors than allowed by default, you may need to use `ulimit -n 12000` or something similar.
