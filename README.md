# stressor

## Compiling

Download Wasmtime

```
https://github.com/bytecodealliance/wasmtime
```

Download a compatible rust version (rustup?)

Compile with testing after compilation

```
cmake -DCMAKE_BUILD_TYPE=Debug -B builds -S . && cmake --build builds && ctest --test-dir ./builds/
```

## Other info (temporary)

WASM modules you provide MUST be stateless (i.e functions). This is likely already the case.
