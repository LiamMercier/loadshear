# stressor

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

## Other info (temporary)

WASM modules you provide MUST be stateless (i.e functions). This is likely already the case.
