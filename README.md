# stressor

## Compiling

Compile with testing after compilation

```
cmake -DCMAKE_BUILD_TYPE=Debug -B builds -S . && cmake --build builds && ctest --test-dir ./builds/
```
