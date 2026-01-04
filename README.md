# Loadshear

TODO: about

## table of contents

TODO: table of contents

## Installation

TODO: installation guide (should be simple when done).

## Quickstart

Lets quickly learn how to use `loadshear` for generating network loads.

Using `loadshear` requires a valid loadshear script. We will use a simple template script to demonstrate functionality. Start by creating a new folder.

```
mkdir ex-loadshear && cd ex-loadshear
```

Make a placeholder packet.

```
printf "hello world" > packet.bin
```

Copy the example template [template.ldsh](sdk/scripts/template.ldsh) into the folder.

Now, view the execution plan using the dry run flag.

```
loadshear template.ldsh --dry-run
```

Or, try running the script against your loopback address.

```
loadshear template.ldsh
```

## Loadshear Scripting Language

Loadshear uses a simple Domain Specific Language (DSL) named Loadshear Script for customizing load generation. The `loadshear` application will use any file that follows the DSL, but we suggest naming files with `.ldsh` or `.loadshear` for workspace cleanliness. Examples can be found in the [sdk](sdk) folder.

### Syntax

Valid scripts are expected to have:

- One `SETTINGS` block, defining any external resources
- One `ORCHESTRATOR` block, defining executable actions

In the event that there are multiple blocks the program may output an error or use the last block as being canonical. Prefer having one pair of blocks per script.

> Note: In the future, we may extend the DSL to allow for multiple block pairs.

For syntax specifics, see [syntax](docs/syntax.md).

### Execution Model

High level blocks are parsed to create an execution plan which can be previewed with the flag `--dry-run` from the CLI. 

During plan generation, we read each packet defined in `PACKETS` into memory, and generate a small payload descriptor for each `SEND` copy that was defined.

We parse the actions linearly in the ORCHESTRATOR block to construct a list of actions to execute at runtime, certain actions are required for others to be valid (i.e CREATE before CONNECT). Right after executing the final action, the orchestrator will set all resources as being able to shutdown and issue closing commands.

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

For specifics on creating a compatible WASM module, see [wasm-modules](docs/wasm-modules.md)

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

Go to the example directory [wasm](sdk/wasm) and build the examples.

```
make
```

If you have the WebAssembly Binary Toolkit (WABT) installed, strip metadata.

```
make strip
```

Some tests are hidden behind an environment variable. You can run these with `RUN_HEAVY_GTEST=1` if you wish. Some tests may require more file descriptors than allowed by default, you may need to use `ulimit -n 12000` or something similar.

## Backlog

- Have SessionPool hold shared memory for transports instead of unique memory buffers
- Refactor each CMake subtarget to have modern include semantics
- Show endpoints/misc in dry run
- Setup LTO in cmake for release builds
