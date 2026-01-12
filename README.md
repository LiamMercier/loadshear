# Loadshear

Loadshear is a customizable load generator designed for high-concurrency testing of TCP and UDP endpoints.

## Responsible Use

Loadshear is capable of generating high network loads, rapid connection churn, and resource exhaustion if misused. It is designed **only** for use on systems for which you have **explicit authorization** to act on.

Misuse use of this tool can cause service disruption and may have legal consequences. If you are unsure of whether you are authorized to use Loadshear on an endpoint, stop immediately.

## Table of Contents

- [Installation](#installation)
    - [Linux (Debian-based)](#linux-debian-based)
    - [Linux (RPM-based)](#linux-rpm-based)
    - [Verifying Packages](#verifying-packages)
- [Quickstart](#quickstart)
- [Usage Guide](#usage-guide)
- [Loadshear Scripting Language](#loadshear-scripting-language)
    - [Syntax](#syntax)
    - [Execution Model](#execution-model)
- [Defining the Packet Response Protocol](#defining-the-packet-response-protocol)
    - [Message Handlers](#message-handlers)
- [Development](#development)
    - [Compiling](#compiling)
    - [Running Tests](#running-tests)

## Installation

### Linux (Debian-based)

Download the Debian package (.deb), optionally verify the package (see [Verifying Packages](#verifying-packages)), then run

```
sudo apt install ./loadshear-1.0.0.deb
```

### Linux (RPM-based)

Download the RPM package (.rpm), optionally verify the package (see [Verifying Packages](#verifying-packages)), then run

```
sudo dnf install ./loadshear-1.0.0.rpm
```

### Linux (General)

If your package manager is not supported, you can try manually installing Loadshear using the provided tarball. 

### Verifying Packages

All releases of Loadshear have a list of signed SHA256 checksums for each file. Packages are verified by ensuring that the checksums for the files you download are strictly equal to the signed checksums.

First, download and import the following [public key](github.com/LiamMercier/LiamMercier)

```
gpg --import public.asc
```

The resulting message should produce a fingerprint (16 bytes) which matches the end of the full key fingerprint, `FF350E63EA2664FB346FA56081B2CF5109324EFC`

If the fingerprint matches, verify the file signature

```
gpg --verify checksums.asc
```

This will likely output a message containing the following:

```
Good signature from "Liam Mercier <LiamMercier@proton.me>" [unknown]
WARNING: This key is not certified with a trusted signature!
         There is no indication that the signature belongs to the owner.
Primary key fingerprint: FF35 0E63 EA26 64FB 346F A560 81B2 CF51 0932 4EFC
```

This is expected unless you certified the public key after importing.

If verification is successful, compare the SHA256 checksum of the files you downloaded to the signed checksum values, they should match exactly. On Linux you can use the following to get the checksum for all files in the current directory.

```
sha256sum ./*
```

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

## Usage Guide

Loadshear provides various metrics to help assess the current test performance. Users should read the [usage guide](docs/usage.md) for information on interpreting metrics.

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

The application assumes that for incoming TCP packets are started with a fixed size header containing the body size of the packet. For example, a packet might have the first 8 bytes defining the packet type and body length, and then the body.

If your protocol uses variable sized headers, you must declare HEADERSIZE such that it will always contain the size field to extract the body.

For TCP, each session instance (with reading enabled) will:

- 1. Try to read HEADERSIZE bytes from the socket
- 2. Try to get the packet's body_size by calling the user defined handle_header function on the bytes read
- 3. Try to read body_size bytes from the socket to get the body
- 4. Try to generate a response by giving the header and body as contiguous memory to the defined MessageHandler instance
- 5. Send the response and restart the loop

As for UDP packets, the entire datagram is given to the message handler.

Each session (with reading enabled) will:

- 1. Try to read a datagram from the socket
- 2. Try to generate a response by passing the datagram as contiguous memory to the defined MessageHandler instance
- 3. Send the response and restart the loop

### Message Handlers

Each Session that reads messages MUST be assigned a MessageHandler.

There are two options for HANDLER to use:

1) Set HANDLER = "NOP" in your script. The session will clear incoming data without invoking response logic.

2) Use a WebAssembly (WASM) module passed to HANDLER as a file path, just like any packets you have defined. To use the WASM handler, you must provide:

- A valid .wasm module that defines how to construct the response
- A function in the module to read the body size from headers

For specifics on creating a compatible WASM module, see [wasm-modules](docs/wasm-modules.md)

## Development

The following documents useful information for developers or power users who wish to compile Loadshear.

### Compiling

Download build tools and libraries, the following is for Debian based systems:

```
sudo apt install build-essential cmake libboost-all-dev
```

Download Wasmtime, as of writing it seems they are using a shell script. Follow their instructions for installation.

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

Compile the binary for release and create packages

```
cmake --preset release && cmake --build --preset release-multi --target package
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
- Setup LTO in cmake for release builds
- Metrics for packets sent alongside bytes sent
- Churning utils
- Profile various optimizations denoted in code but not yet tested
- Design documents for development section
