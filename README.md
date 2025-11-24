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
