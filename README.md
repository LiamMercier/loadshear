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

## Creating WASM modules

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
